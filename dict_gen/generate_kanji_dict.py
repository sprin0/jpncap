#!/usr/bin/env python3

import argparse
from sys import exit, stdout
from os import path
import sqlite3
import xml.etree.ElementTree as ET

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

argparser = argparse.ArgumentParser(description="Generate a kanji dictionary table from the XML source file. Download at http://www.edrdg.org/kanjidic/kanjidic2.xml.gz")
argparser.add_argument("input_file", help="path to the input XML file")
argparser.add_argument("output_file", help="path to the output sqlite file")

args = argparser.parse_args()

input_size = path.getsize(args.input_file)
input_file = open(args.input_file)

stdout.write("Creating kanji dictionary database file...\r")
stdout.flush()
conn = sqlite3.connect(args.output_file)
conn.isolation_level = None
c = conn.cursor()

if table_exists(c, "kanjidict_toc") or table_exists(c, "kanjidict"):
	print("Kanji table already exists in output file.")
	exit(0)

try:
	c.execute("SAVEPOINT start_savepoint;")
	c.execute("CREATE TABLE IF NOT EXISTS Languages ( 'id' INTEGER PRIMARY KEY AUTOINCREMENT, 'display_name' TEXT, 'table_name' TEXT, 'column_name' TEXT, 'deinflect' INTEGER );")
	c.execute("CREATE TABLE kanjidict_toc ( 'id' INTEGER PRIMARY KEY AUTOINCREMENT, 'word' TEXT, 'ent_id' INTEGER );")
	c.execute("CREATE TABLE kanjidict ( 'id' INTEGER PRIMARY KEY, 'japanese' TEXT, 'pos' TEXT, 'english' TEXT );")
	c.execute("INSERT INTO Languages (display_name, table_name, column_name, deinflect) VALUES ('Kanji dictionary', 'kanjidict', 'english', 0);")

	for event, entry in ET.iterparse(input_file, events=("end",)):
		stdout.write("Creating kanji dictionary database file...{}%\r".format(round(input_file.tell() / input_size * 100)))
		stdout.flush()
		
		if entry.tag != "character":
			continue
		for literal_tag in entry.iter("literal"):
			id = ord(literal_tag.text)
			literal = literal_tag.text
			c.execute("INSERT INTO kanjidict_toc ('word', 'ent_id') VALUES (?, ?);", (literal, id))
			
		info = []
		for grade_tag in entry.iter("grade"):
			info.append("Grade " + grade_tag.text)
		for stroke_count_tag in entry.iter("stroke_count"):
			info.append(stroke_count_tag.text + " Strokes")
		for freq_tag in entry.iter("freq"):
			info.append("Frequency " + freq_tag.text)
		for jlpt_tag in entry.iter("jlpt"):
			info.append("JLPT " + jlpt_tag.text)
			
		reading = []
		for reading_tag in entry.iter("reading"):
			if "r_type" in reading_tag.attrib:
				if reading_tag.attrib["r_type"] == "ja_on" or reading_tag.attrib["r_type"] == "ja_kun":
					reading.append(reading_tag.text)
					c.execute("INSERT INTO kanjidict_toc ('word', 'ent_id') VALUES (?, ?);", (katakana2hiragana(reading_tag.text), id))
			
		meaning_eng = []
		for meaning_tag in entry.iter("meaning"):
			if len(meaning_tag.attrib) == 0:
				meaning_eng.append(meaning_tag.text)
				
		nanori = []
		for nanori_tag in entry.iter("nanori"):
			nanori.append(nanori_tag.text)
			
		japanese = literal
		if len(reading) > 0:
			japanese += "\n" + ", ".join(reading)
		if len(nanori) > 0:
			japanese += "\n" + u"名乗り: " + ", ".join(nanori)
		info = ", ".join(info)
		meaning_eng = "; ".join(meaning_eng)
		
		c.execute("INSERT INTO kanjidict ('id', 'japanese', 'pos', 'english') VALUES (?, ?, ?, ?);", (id, japanese, info, meaning_eng))
			
		entry.clear()
		
	c.execute("CREATE INDEX idx_kanjidict ON kanjidict_toc(word);")
	print("\nDone")
except:
	print("\nEncountered expection. Rolling back.")
	c.execute("ROLLBACK TRANSACTION TO SAVEPOINT start_savepoint;")
	raise
finally:
	conn.commit()
	conn.close()
