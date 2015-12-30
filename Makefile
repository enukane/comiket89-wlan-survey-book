NAME = c89book
TARGETPDF = $(NAME).pdf
BUILDPDFDIR = $(NAME)-pdf
TARGETXML = $(NAME).xml
BUILDXMLDIR = $(NAME)-xml

CONFIG = $(NAME).yaml

# add new review file here
DEPFILES = 00-introduction.re \
	   01-capturing.re \
	   02-tochka.re \
	   03-aggr-analyze.re \
	   04-analysis.re \
	   99-conclusion.re

# command
PDFMAKE := review-pdfmaker
XMLMAKE := review-compile --target idgxml

$(TARGETPDF): $(CONFIG) $(DEPFILES)
	rm $(TARGETPDF) 2>/dev/null || echo "no pdf exists: clean build"
	rm -r $(BUILDPDFDIR) 2>/dev/null || echo "no builddir exists: clean build"
	$(PDFMAKE) $(CONFIG)

$(TARGETXML): $(CONFIG) $(DEPFILES)
	rm $(TARGETXML) 2>/dev/null || echo "no xml exists: clean build"
	rm -r $(BUILDXMLDIR) 2>/dev/null || echo "no builddir exists: clean build"
	$(XMLMAKE) 

clean:
	rm $(TARGETPDF) 2>/dev/null || echo "cleaning but no pdf there"
	rm -r $(BUILDDIR) 2>/dev/null || echo "no builddir exists: clean build"
