#!/bin/bash
FATSIM=$1
echo "Generating FAT with 4098 clusters, 256 bytes per cluster";
$FATSIM -g 4098 256;
FATSIM=`echo "$FATSIM empty.fat"`;

execute() 
{ 
   echo $@;
   $@;
   read -n 1 -s;
}

execute $FATSIM -p
execute $FATSIM -m test /           
execute $FATSIM -a bad1.txt /test/   
execute $FATSIM -a bad1.txt /test/    
execute $FATSIM -m bad1.txt /test/    
execute $FATSIM -a bad3.txt /test    
execute $FATSIM -a toolongfileee.txt /test   
execute $FATSIM -m toolongfile.dir /test  
execute $FATSIM -p              
execute $FATSIM -c /test/bad1.txt
execute $FATSIM -l /test/bad1.txt
execute $FATSIM -c /test/bad1.txt 
execute $FATSIM -l /test/bad1.txt 
execute $FATSIM -p 
execute $FATSIM -c /test/bad3.txt
execute $FATSIM -l /test/bad3.txt
execute $FATSIM -c /test/bad3.txt 
execute $FATSIM -l /test/bad3.txt
execute $FATSIM -p            
execute $FATSIM -r /test/bad1.txt
execute $FATSIM -f /test/bad1.txt  
execute $FATSIM -p            
execute $FATSIM -a small.txt /   
execute $FATSIM -c /small.txt           
execute $FATSIM -p         
echo "$FATSIM -l /small.txt > out.txt"
$FATSIM -l /small.txt > out.txt
execute diff small.txt out.txt
execute $FATSIM -r /test/
execute $FATSIM -f /test/bad3.txt
execute $FATSIM -r /test/    
execute $FATSIM -a bad1.txt /           
execute $FATSIM -p          


