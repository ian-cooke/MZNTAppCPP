import struct
import sys
import os

if __name__ == '__main__':
    filename = sys.argv[1]
    printSetting = 0
    if( len(sys.argv)> 2 ):
        if(sys.argv[2] == "print"):
            printSetting = 1

    print("Opening: "+filename)
    fileSize = os.path.getsize(filename)
    print(" File size: " + str(fileSize) )
    numErrors = 0

    with open(filename,"rb") as binFile:
        firstIter = 1
        lastData = 0
        index = 0
        while True:
            data = binFile.read(2)

            if(not data):
                break

            data = struct.unpack("<H",data)[0]

            if(printSetting == 1):
                print(str(index)+": "+str(data))

            if(not firstIter):
                if(data-lastData != 1):
                    if(data != 0):
                        print("Error at: "+ str(index))
                        print("Last: "+str(lastData))
                        print("Curr: "+str(data))
                        numErrors = numErrors + 1

            else:
                print("First number: " + str(data))
                firstIter = 0


            lastData = data
            index += 1
            
    print("Num errors: " + str(numErrors))
