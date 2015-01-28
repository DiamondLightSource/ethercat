HTMLFLAGS = --book --duplex --no-numbered --format pdf14 --title --size letter

%.pdf: %.html
	-htmldoc $(HTMLFLAGS) --outfile $@ $<

all: doxygen install

doxygen:
	doxygen

install:
	mkdir -p ../html
	-cp *.html ../html
	-cp *.jpg ../html
	-cp *.png ../html
	-cp *.pdf ../html
	cp -r ethercatDoxygenHTML ../html

clean:

realclean: clean
	rm -f *.pdf
