import sys
import re

BUFFER_SIZE = 2 * 1024 * 1024
OUTPUT_BUFFER_SIZE = BUFFER_SIZE // 60 * 60

t = bytes.maketrans(b'ACGTUMRWSYKVHDBNacgtumrwsykvhdbn', b'TGCAAKYWSRMBDHVNTGCAAKYWSRMBDHVN')

lastCaption = b'';
lastSection = [];
    
def FinalizeSection():
    global lastSection, lastCaption
    if len(lastSection) == 0:
        return

    sys.stdout.buffer.write(b'>' + lastCaption + b'\n')

    wholeSection = b''.join(lastSection)
    lastSection = []
    for i in range( len(wholeSection) // OUTPUT_BUFFER_SIZE + 1 ):
        slice = wholeSection[i * OUTPUT_BUFFER_SIZE: (i+1) * OUTPUT_BUFFER_SIZE]
        if len(slice) > 0:
            arr = [slice[start:start+60] for start in range(0, len(slice), 60)]
            sys.stdout.buffer.write(b'\n'.join(arr))
            sys.stdout.buffer.write(b'\n')
# FinalizeSection end
    
def AppendData(input):
    global lastSection
    data = input.replace(b'\n', b'').translate(t)[::-1]
    lastSection.insert(0, data)
# AppendData end

captionSplit = False

while True:
    input = sys.stdin.buffer.read(BUFFER_SIZE)
    if len(input) == 0:
        break
    parts = re.split(b'(>)', input)
    nextProcessed = False
    for i in range(len(parts)):
        if captionSplit:
            captionSplit = False
            part = parts[i]
            captionEnd = part.find(b'\n')
            assert captionEnd != -1 # 2 MB+ caption?
            lastCaption += part[:captionEnd]
            parts[i] = part[captionEnd+1:]
        if nextProcessed:
            nextProcessed = False
            continue
        if len(parts[i]) == 0:
            continue
    
        if parts[i] == b'>':
            FinalizeSection()
            part = parts[i+1]
            nextProcessed = True
            
            captionEnd = part.find(b'\n')
            if captionEnd == -1:
                captionSplit = True # an end of the caption resides in the next input buffer
                lastCaption = part
            else:
                lastCaption = part[:captionEnd]
                
            data = part[captionEnd+1:]
        else:
            data = parts[i]

        AppendData(data)

if len(lastSection) > 0:    
    FinalizeSection()

import os
os.system('procstat -r {}'.format(os.getpid()))