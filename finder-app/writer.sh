#!/bin/bash

# Check if both write file path and write string are specified
if [ "$#" -ne 2 ]
then
    echo "Error: writefile and writestr, both must be specified."
    exit 1
fi

writefile="$1"
writestr="$2"


# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Write the specified string to the file
echo "$writestr" > "$writefile"

# Check if file creation was successful
if [ $? -ne 0 ]
then
    echo "Error: Could not create file $writefile."
    exit 1
fi

echo "File created successfully: $writefile"
