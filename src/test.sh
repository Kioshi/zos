#!/bin/bash
set -e

if [ $# -eq 0 ] ; then
    echo "No arguments supplied, need path to FATSym executable" ;
    exit 1;
fi
if [[ -x "$1" ]] ; then
  FATSIM=$1
else
  echo "$1 is not executable!";
  exit 1;
fi
FATSIM=$1
echo "Generating FAT with 4098 clusters, 256 bytes per cluster";
$FATSIM -g 4098 256;
FATSIM=`echo "$FATSIM empty.fat"`;

execute() 
{ 
   echo "----------------";
   echo $@;
   $@;      
   echo " ";
   read -n 1 -s;
}

execute $FATSIM -p
execute $FATSIM -m test /           
execute $FATSIM -a small.txt /test/
execute $FATSIM -a toolongfileee.txt /test   
execute $FATSIM -m toolongfile.dir /test  
execute $FATSIM -p              
execute $FATSIM -x              
execute $FATSIM -c /test/small.txt
execute $FATSIM -l /test/small.txt
execute $FATSIM -b 2             
execute $FATSIM -b 4             
execute $FATSIM -b 9             
execute $FATSIM -l /test/small.txt
execute $FATSIM -c /test/small.txt 
execute $FATSIM -p                   
execute $FATSIM -x              
#execute $FATSIM -l /test/small.txt
execute $FATSIM -b 1    
execute $FATSIM -p                   
execute $FATSIM -x    
execute $FATSIM -r /test/
execute $FATSIM -f /test/fffffffft
execute $FATSIM -r /test/    
execute $FATSIM -p                   
execute $FATSIM -x    
execute $FATSIM -a big.txt /test/    
execute $FATSIM -a big.txt /    
execute $FATSIM -p                   
execute $FATSIM -x       
echo "----------------";
echo "$FATSIM -l /big.txt > out.txt"   
echo " ";
$FATSIM -l /big.txt > out.txt
execute diff big.txt out.txt
          


