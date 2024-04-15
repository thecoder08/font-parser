#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <malloc.h>

#include <xgfx/window.h>
#include <xgfx/drawing.h>

typedef struct {
    char tag[4];
    unsigned int checkSum;
    unsigned int offset;
    unsigned int length;
} TableDirectoryEntry;

typedef struct {
    short numContours;
    unsigned short* contourEndIndices;
    unsigned short numPoints;
    int* xCoordinates;
    int* yCoordinates;
    unsigned char* pointFlags;
} Glyph;

int fontFile;

unsigned short readOffsetSubtable() {
    unsigned int scalar;
    read(fontFile, &scalar, 4);
    scalar = ntohl(scalar);
    unsigned short numTables;
    read(fontFile, &numTables, 2);
    numTables = ntohs(numTables);
    printf("Number of tables: %u\n", numTables);
    lseek(fontFile, 6, SEEK_CUR);
    return numTables;
}

TableDirectoryEntry readTableDirectoryEntry() {
    TableDirectoryEntry entry;
    read(fontFile, entry.tag, 4);
    printf("Table %.4s:\n", entry.tag);
    read(fontFile, &entry.checkSum, 4);
    entry.checkSum = ntohl(entry.checkSum);
    printf("Checksum: %u\n", entry.checkSum);
    read(fontFile, &entry.offset, 4);
    entry.offset = ntohl(entry.offset);
    printf("Offset: %u\n", entry.offset);
    read(fontFile, &entry.length, 4);
    entry.length = ntohl(entry.length);
    printf("Length: %u\n", entry.length);
    return entry;
}

Glyph readGlyph() {
    Glyph glyph;
    read(fontFile, &glyph.numContours, 2);
    glyph.numContours = ntohs(glyph.numContours);
    printf("Number of contours: %d\n", glyph.numContours);
    short xMin;
    read(fontFile, &xMin, 2);
    xMin = ntohs(xMin);
    printf("Minimum X: %d\n", xMin);
    short yMin;
    read(fontFile, &yMin, 2);
    yMin = ntohs(yMin);
    printf("Minimum Y: %d\n", yMin);
    short xMax;
    read(fontFile, &xMax, 2);
    xMax = ntohs(xMax);
    printf("Maximum X: %d\n", xMax);
    short yMax;
    read(fontFile, &yMax, 2);
    yMax = ntohs(yMax);
    printf("Maximum Y: %d\n", yMax);
    printf("Contour End Indices:");
    glyph.contourEndIndices = malloc(sizeof(unsigned short) * glyph.numContours);
    read(fontFile, glyph.contourEndIndices, sizeof(unsigned short) * glyph.numContours);
    for (int i = 0; i < glyph.numContours; i++) {
        glyph.contourEndIndices[i] = ntohs(glyph.contourEndIndices[i]);
        printf(" %u", glyph.contourEndIndices[i]);
    }
    printf("\n");
    glyph.numPoints = glyph.contourEndIndices[glyph.numContours - 1] + 1; // clever, sebastian! The last element of the glyph.contourEndIndices array is the index of the last point in the glyph; add 1 to get the length!
    printf("Number of points: %u\n", glyph.numPoints);
    unsigned short instructionLength;
    read(fontFile, &instructionLength, 2);
    instructionLength = ntohs(instructionLength);
    lseek(fontFile, instructionLength, SEEK_CUR);

    glyph.pointFlags = malloc(sizeof(unsigned char) * glyph.numPoints);
    printf("Flags:");
    for (int i = 0; i < glyph.numPoints; i++) {
        unsigned char flag;
        read(fontFile, &flag, 1);
        glyph.pointFlags[i] = flag;
        printf(" 0x%x", flag);
        if (flag & 0x08) { // repeat flag byte
            unsigned char numRepeats;
            read(fontFile, &numRepeats, 1);
            for (int j = 0; j < numRepeats; j++) {
                i++;
                glyph.pointFlags[i] = flag;
                printf(" 0x%x", flag);
            }
        }
    }
    printf("\n");

    glyph.xCoordinates = malloc(sizeof(int) * glyph.numPoints);
    for (int i = 0; i < glyph.numPoints; i++) {
        glyph.xCoordinates[i] = 0;
        if (i > 0) {
            glyph.xCoordinates[i] = glyph.xCoordinates[i-1];
        }
        if (glyph.pointFlags[i] & 0x02) {
            // x-coordinate is 1 byte
            unsigned char xCoord;
            read(fontFile, &xCoord, 1);
            int sign = (glyph.pointFlags[i] & 0x10) ? 1 : -1;
            glyph.xCoordinates[i] += xCoord * sign;

        }
        else if (!(glyph.pointFlags[i] & 0x10)) {
            // x-coordinate is 2 bytes and we aren't skipping it
            short xCoord;
            read(fontFile, &xCoord, 2);
            xCoord = ntohs(xCoord);
            glyph.xCoordinates[i] += xCoord;
        }
    }

    glyph.yCoordinates = malloc(sizeof(int) * glyph.numPoints);
    for (int i = 0; i < glyph.numPoints; i++) {
        glyph.yCoordinates[i] = 0;
        if (i > 0) {
            glyph.yCoordinates[i] = glyph.yCoordinates[i-1];
        }
        if (glyph.pointFlags[i] & 0x04) {
            // y-coordinate is 1 byte
            unsigned char yCoord;
            read(fontFile, &yCoord, 1);
            int sign = (glyph.pointFlags[i] & 0x20) ? 1 : -1;
            glyph.yCoordinates[i] += yCoord * sign;

        }
        else if (!(glyph.pointFlags[i] & 0x20)) {
            // y-coordinate is 2 bytes and we aren't skipping it
            short yCoord;
            read(fontFile, &yCoord, 2);
            yCoord = ntohs(yCoord);
            glyph.yCoordinates[i] += yCoord;
        }
    }

    printf("Points:\n");
    for (int i = 0; i < glyph.numPoints; i++) {
        printf("(%d, %d)\n", glyph.xCoordinates[i], glyph.yCoordinates[i]);
    }

    return glyph;
}

void readGlyphTable(unsigned int offset, Glyph* glyphTable) {
    printf("\n");
    lseek(fontFile, offset, SEEK_SET);
    for (int i = 0; i < 2; i++) {
        glyphTable[i] = readGlyph();
    }
    printf("\n");
}

int main() {
    fontFile = open("DroidSansMono.ttf", O_RDONLY);
    if (fontFile == -1) {
        fprintf(stderr, "Error opening font file!\n");
        return 1;
    }
    
    Glyph glyphs[2];

    int numTables = readOffsetSubtable();
    for (int i = 0; i < numTables; i++) {
        lseek(fontFile, i*16+12, SEEK_SET);
        TableDirectoryEntry entry = readTableDirectoryEntry();
        if (memcmp(entry.tag, "loca", 4) == 0) {
            readLocationTable(entry.offset);
        }
        else if (memcmp(entry.tag, "glyf", 4) == 0) {
            readGlyphTable(entry.offset, glyphs);
        }
    }

    initWindow(640, 480, "Font Renderer");
    while (1) {
        Event event;
        while(checkWindowEvent(&event)) {
            if (event.type == WINDOW_CLOSE) {
                return 0;
            }
        }
        rectangle(0, 0, 640, 480, 0x00000000);
        // draw glyphs
        for (int i = 0; i < 2; i++) {
            int startIndex = 1;
            for (int j = 0; j < glyphs[i].numContours; j++) {
                for (int k = startIndex; k <= glyphs[i].contourEndIndices[j]; k++) {
                    line(glyphs[i].xCoordinates[k-1]/5, glyphs[i].yCoordinates[k-1]/5, glyphs[i].xCoordinates[k]/5, glyphs[i].yCoordinates[k]/5, 0xffffffff);
                }
                line(glyphs[i].xCoordinates[startIndex-1]/5, glyphs[i].yCoordinates[startIndex-1]/5, glyphs[i].xCoordinates[glyphs[i].contourEndIndices[j]]/5, glyphs[i].yCoordinates[glyphs[i].contourEndIndices[j]]/5, 0xffffffff);
                startIndex = glyphs[i].contourEndIndices[j] + 2;
            }
        }
        updateWindow();
    }
}
