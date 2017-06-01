PY=python

all: meld

init6.py: parser.yml
	kaitai-struct-compiler -t python parser.yml

parsed1.txt: init6.py parser.py
	$(PY) parser.py bin init6_1.bin > parsed1.txt

parsed2.txt: init6.py parser.py
	$(PY) parser.py txt init6_2.txt > parsed2.txt

meld: parsed1.txt parsed2.txt
	meld parsed1.txt parsed2.txt

clean:
	rm -rf init6.py parsed*.txt __pycache__
