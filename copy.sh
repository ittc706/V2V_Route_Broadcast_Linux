#! /bin/bash

project="V2V_Route_Broadcast"
project_linux="V2V_Route_Broadcast_Linux"

if [ ! -d ../${project}/${project} ];then
	echo "Please place ${project} and ${project_linux} in the same directory"
	exit
fi

# First obtain the ${project}'s git version number

cd ../${project}/${project}/
uq=$(git rev-list --all | head -n 1)
uq=${uq:0:7}

# Write the format version information

description="## version_"$(date +%Y)"_"$(date +%m)"_"$(date +%d)"("$uq")"

cd ../../${project_linux}

echo $description >> README.md

# Remove the old files
rm -f *.h *.cpp
rm -rf config log wt reflect

sleep 2s

# Copy files from ${project}
cp -a ../${project}/${project}/*.cpp  ../${project}/${project}/*.h  .
cp -a ../${project}/${project}/config ../${project}/${project}/log ../${project}/${project}/wt ../${project}/${project}/reflect .

sleep 2s

# Begin converting the encoding format

files=$(find . -regex ".*\.\(h\|cpp\)")

for file in $files
do
	echo "Convert encoding format: "$file
	iconv -f gbk -t utf-8 ${file} > ${file}.tmp
	rm -f ${file}
	mv ${file}.tmp  ${file}
done

# Commit

git add .

git commit -m ${description:3}

git push -f origin master:master
