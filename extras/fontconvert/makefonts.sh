#!/bin/bash

# Ugly little Bash script, generates a set of .h files for GFX using
# GNU FreeFont sources.  There are three fonts: 'Mono' (Courier-like),
# 'Sans' (Helvetica-like) and 'Serif' (Times-like); four styles: regular,
# bold, oblique or italic, and bold+oblique or bold+italic; and four
# sizes: 9, 12, 18 and 24 point.  No real error checking or anything,
# this just powers through all the combinations, calling the fontconvert
# utility and redirecting the output to a .h file for each combo.

# Adafruit_GFX repository does not include the source outline fonts
# (huge zipfile, different license) but they're easily acquired:
# http://savannah.gnu.org/projects/freefont/

include=includefont.h
convert=./fontconvert
inpath=./freefont/
outpath=../Fonts/
fonts=(FreeMono FreeSans FreeSerif MonoFonto UbuntuMono)
styles=("" Bold Italic BoldItalic Oblique BoldOblique Regular)
sizes=(8 9 10 11 12 18 24 30 40 45 54)
includefile=$outpath$include
echo "//Autogenerate By Makefonts.sh" > $includefile
echo "#ifndef INCLUDEFONT_H" >> $includefile
echo "#define INCLUDEFONT_H" >> $includefile


for f in ${fonts[*]}
do
	for index in ${!styles[*]}
	do
		st=${styles[$index]}
		for si in ${sizes[*]}
		do
			infile=$inpath$f$st".ttf"
			if [ -f $infile ] # Does source combination exist?
			  then
				outfile=$outpath$f$st$si"pt7b.h"
				printf "%s %s %s > %s\n" $convert $infile $si $outfile
				$convert $infile $si > $outfile
				#add include file font .h
				
				# ex: #include <Fonts/FreeSans9pt7b.h>
				headerfile="#include <Fonts/"$f$st$si"pt7b.h>"
				echo $headerfile >> $includefile
			fi
		done
	done
done
echo "#endif" >> $includefile