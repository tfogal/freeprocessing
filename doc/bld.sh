a2x --icons -L -f pdf freeprocessor.adoc || exit 1
asciidoc -a icons freeprocessor.adoc || exit 1
str="s,tfogal@sci.utah.edu,tfogal@sci,g"
sed -i ${str} freeprocessor.html
