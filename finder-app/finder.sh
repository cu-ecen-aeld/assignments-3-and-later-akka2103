#!/bin/sh

#check if both the arguments are specified
if [ "$#" -ne 2 ]
then
	echo "Error: File directory and search string, both must be specified"
	exit 1
fi

#take the arguments from user
filedir="$1"
searchstr="$2"

#check if the directory exists
if [ ! -d "$filedir" ]
then
	echo "Error: Directory - $filedir dose'nt exists"
	exit 1
fi

#counting total files in specified directory
filecount=$(find "$filedir" -type f | wc -l)

#counting number of matching lines with string passed
stringmatch=$(grep -r "$searchstr" "$filedir" | wc -l)

echo "The number of files are $filecount and the number of matching lines are $stringmatch"

