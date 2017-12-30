#!/usr/bin/env python3

import argparse
from sys import exit, stdout
from os import path
import sqlite3
import xml.etree.ElementTree as ET
from tempfile import TemporaryFile

def katakana2hiragana(text):
	text2 = ""
	for i, character in enumerate(text):
		code = ord(character)
		if 0x30A1 <= code <= 0x30F6:
			text2 += chr(code - 0x60)
		else:
			text2 += character
	return text2
	
def table_exists(cursor, table_name):
	cursor.execute("SELECT name FROM sqlite_master where type='table' and name=?", (table_name,))
	return not cursor.fetchone() is None

AVAILABLE_LANGS      = ["English", "French", "German", "Russian", "Dutch", "Spanish", "Hungarian", "Slovenian", "Swedish"] #must be titlecased
AVAILABLE_LANGS_CODE = ["eng",     "fre",    "ger",    "rus",     "dut",   "spa",     "hun",       "slv",       "swe"] #must be SQL escaped and compatible

argparser = argparse.ArgumentParser(description="Generate a Japanese dictionary from the JMDict's XML source file. Download at http://ftp.monash.edu.au/pub/nihongo/JMdict.gz")
argparser.add_argument("input_file", help="path to the input XML file")
argparser.add_argument("output_file", help="path to the output sqlite file")
argparser.add_argument("-l", "--language", action="append", help="a langauge to include. Available languages: {}".format(", ".join(AVAILABLE_LANGS)))

args = argparser.parse_args()

selected_langs = []
selected_langs_code = []
if not args.language:
	print("No valid language suppied. Add at least one with -l.")
	exit(1)
for lang in args.language:
	lang = lang.title()
	if not lang in AVAILABLE_LANGS:
		print("Language not available: '{}'".format(lang))
		exit(1)
	elif lang in selected_langs:
		continue
	else:
		selected_langs.append(lang)
		selected_langs_code.append(AVAILABLE_LANGS_CODE[AVAILABLE_LANGS.index(lang)])

input_size = path.getsize(args.input_file)
input_file = open(args.input_file, "rb")

#Use abbreviations instead of full explanations by replacing the entities
#by creating a modified XML file
stdout.write("Replacing entities in JMDict XML file...\r")
stdout.flush()
tempf = TemporaryFile()
for line in input_file:
	if line[:9] == b"<!ENTITY ":
		e = line.split(b" ")
		tempf.write(e[0] + b" " + e[1] + b" \"" + e[1] + b"\">\n")
	else:
		tempf.write((line) + b"\n")
input_file.close()
input_size = tempf.tell()
tempf.seek(0)
input_file = tempf
            
stdout.write("Creating JM dictionary database file...\r")
stdout.flush()
conn = sqlite3.connect(args.output_file)
conn.isolation_level = None
c = conn.cursor()

if table_exists(c, "jmdict_toc") or table_exists(c, "jmdict"):
	print("jmdict table already exists in output file.")
	conn.close()
	input_file.close()
	exit(0)

try:
	columns = []
	entry_columns = []
	for lang in selected_langs_code:
		columns.append("'{}' TEXT".format(lang))
		entry_columns.append("'{}'".format(lang))
	c.execute("SAVEPOINT start_savepoint;")
	c.execute("CREATE TABLE IF NOT EXISTS Languages ( 'id' INTEGER PRIMARY KEY AUTOINCREMENT, 'display_name' TEXT, 'table_name' TEXT, 'column_name' TEXT, 'deinflect' INTEGER );")
	c.execute("CREATE TABLE jmdict_toc ( 'id' INTEGER PRIMARY KEY AUTOINCREMENT, 'word' TEXT, 'ent_id' INTEGER );")
	c.execute("CREATE TABLE jmdict ( 'id' INTEGER PRIMARY KEY, 'japanese' TEXT, 'pos' TEXT, {} );".format(", ".join(columns)))
	for lang, lang_code in zip(selected_langs, selected_langs_code):
		disp_name = lang + " dictionary"
		c.execute("INSERT INTO Languages (display_name, table_name, column_name, deinflect) VALUES (?, ?, ?, ?);", (disp_name, "jmdict", lang_code, True))
	
	entry_columns = ", ".join(entry_columns)
	for lang in selected_langs_code:
		columns.append("'{}'".format(lang))
	for event, entry in ET.iterparse(input_file, events=("end",)):
		stdout.write("Creating JM dictionary database file...{}%\r".format(round(input_file.tell() / input_size * 100)))
		stdout.flush()
		
		if entry.tag != "entry":
			continue
		ent_seq = int(entry.findtext("ent_seq"))
		for k_ele in entry.iter("k_ele"):
			c.execute("INSERT INTO jmdict_toc ('word', 'ent_id') VALUES (?, ?);", (katakana2hiragana(k_ele.findtext("keb")), ent_seq))
		for r_ele in entry.iter("r_ele"):
			c.execute("INSERT INTO jmdict_toc ('word', 'ent_id') VALUES (?, ?);", (katakana2hiragana(r_ele.findtext("reb")), ent_seq))
			
		japanese = ""
		
		kanji = []
		for k_ele in entry.iter("k_ele"):
			kanji.append(k_ele.findtext("keb"))
		if len(kanji) != 0:
			japanese += (", ").join(kanji)
			japanese += "\t"
			
		readings = []
		for r_ele in entry.iter("r_ele"):
			if r_ele.find("re_restr") != None:
				re_restrs = []
				for re_restr in r_ele.iter("re_restr"):
					re_restrs.append(re_restr.text)
				readings.append(r_ele.findtext("reb") + "(for " + (";").join(re_restrs) + ")")
			else:
				readings.append(r_ele.findtext("reb"))
		if len(readings) != 0:
			japanese += (", ").join(readings)
			
		pos_set = set()
		translations = {}
		senses = {}
		for lang_code in selected_langs_code:
			translations[lang_code] = ""
			senses[lang_code] = []
		for sense in entry.iter("sense"):
			for pos in sense.iter("pos"):
				pos_set.add(pos.text)
			for misc in sense.iter("misc"):
				pos_set.add(misc.text)
			
			xrefs = []
			for xref in sense.iter("xref"):
				xrefs.append(xref.text)
				
			info = []
			for field in sense.iter("field"):
				info.append(field.text)
			for dial in sense.iter("dial"):
				info.append(dial.text)
				
			s_infs = []
			for s_inf in sense.iter("s_inf"):
				s_infs.append(s_inf.text)
				
			glosses = {}
			for lang_code in selected_langs_code:
				glosses[lang_code] = []
			for gloss in sense.iter("gloss"):
				if len(gloss.attrib) == 0:
					lang = "eng"
				else:
					lang = next(iter(gloss.attrib.values())).lower()
				
				if lang in selected_langs_code:
					glosses[lang].append(gloss.text)
					
			pre_sense = ""
			post_sense = ""
			if len(info) != 0:
				pre_sense += "(" + (", ").join(info) + ") "
			if len(s_infs) != 0:
				pre_sense += "(" + (") (").join(s_infs) + ") "
			if len(xrefs) != 0:
				post_sense += "(See " + (", ").join(xrefs) + ")"
			for lang, glosses_lang in glosses.items():
				if len(glosses_lang) > 0:
					senses[lang].append("{} {} {}".format(pre_sense, ("; ").join(glosses_lang), post_sense))
				else:
					senses[lang].append("")
		
		for lang, senses_lang in senses.items():
			for i, sense in enumerate(senses_lang):
				if len(sense) > 0:
					translations[lang] += "({}) {}\n".format(i + 1, sense)
		pos = ("; ").join(pos_set)
		
		column_vals = []
		values = []
		for lang in selected_langs_code:
			column_vals.append(translations[lang])
			values.append("?")
		values = ", ".join(values)
		c.execute("INSERT INTO jmdict ('id', 'japanese', 'pos', {}) VALUES (?, ?, ?, {});".format(entry_columns, values), (ent_seq, japanese, pos) + tuple(column_vals))
		entry.clear()
			
	c.execute("CREATE INDEX idx_word ON jmdict_toc(word);")
	print("\nDone")
except:
	print("\nEncountered expection. Rolling back.")
	c.execute("ROLLBACK TRANSACTION TO SAVEPOINT start_savepoint;")
	raise
finally:
	conn.commit()
	conn.close()
