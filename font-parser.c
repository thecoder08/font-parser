#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
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

typedef struct {
    int x;
    int y;
} Point;

int fontFile;
int numTables;
TableDirectoryEntry* tableDirectory;
Glyph* glyphs;
unsigned short* locations;
unsigned short numGlyphs;

unsigned short ntohs(unsigned short in);
unsigned int ntohl(unsigned int in);
void drawBezier(Point a, Point b, Point c, int color);

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

TableDirectoryEntry getTable(char* tag) {
    for (int i = 0; i < numTables; i++) {
        if (memcmp(tableDirectory[i].tag, tag, 4) == 0) {
            return tableDirectory[i];
        }
    }
}

unsigned short readNumGlyphs(unsigned int offset) {
    lseek(fontFile, offset + 4, SEEK_SET);
    unsigned short numGlyph;
    read(fontFile, &numGlyph, 2);
    numGlyph = ntohs(numGlyph);
    return numGlyph;
}

Glyph readGlyph(unsigned int offset) {
    lseek(fontFile, offset, SEEK_SET);
    Glyph glyph;
    read(fontFile, &glyph.numContours, 2);
    glyph.numContours = ntohs(glyph.numContours);
    printf("Number of contours: %d\n", glyph.numContours);
    if (glyph.numContours < 0) {
        return glyphs[0];
    }
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

void readGlyphTable(unsigned int offset) {
    printf("\n");
    lseek(fontFile, offset, SEEK_SET);
    for (int i = 0; i < numGlyphs; i++) {
        printf("glyph #%d/%d:\n", i+1, numGlyphs);
        glyphs[i] = readGlyph(offset + locations[i]);
    }
    printf("\n");
}

short readLocationFormat(unsigned int offset) {
    lseek(fontFile, offset + 50, SEEK_SET);
    short locFormat;
    read(fontFile, &locFormat, 2);
    locFormat = ntohs(locFormat);
    return locFormat;
}

void readShortLocationTable(unsigned int offset) {
    lseek(fontFile, offset, SEEK_SET);
    for (int i = 0; i < numGlyphs + 1; i++) {
        unsigned short glyphOffset;
        read(fontFile, &glyphOffset, 2);
        glyphOffset = ntohs(glyphOffset);
        locations[i] = glyphOffset * 2;
        printf("%d: %d\n", i, locations[i]);
    }
}

void readLongLocationTable(unsigned int offset) {
    lseek(fontFile, offset, SEEK_SET);
    for (int i = 0; i < numGlyphs + 1; i++) {
        unsigned int glyphOffset;
        read(fontFile, &glyphOffset, 4);
        glyphOffset = ntohl(glyphOffset);
        locations[i] = glyphOffset;
    }
}

int main() {
    fontFile = open("DroidSansMono.ttf", O_RDONLY);
    if (fontFile == -1) {
        fprintf(stderr, "Error opening font file!\n");
        return 1;
    }
    
    numTables = readOffsetSubtable();
    tableDirectory = malloc(numTables * sizeof(TableDirectoryEntry));
    for (int i = 0; i < numTables; i++) {
        tableDirectory[i] = readTableDirectoryEntry();
    }

    short locFormat = readLocationFormat(getTable("head").offset);
    printf("Location table format: %d\n", locFormat);
    numGlyphs = readNumGlyphs(getTable("maxp").offset);
    numGlyphs = 100;
    glyphs = malloc(numGlyphs * sizeof(Glyph));
    locations = malloc(numGlyphs * sizeof(unsigned int));

    if (locFormat == 0) {
        readShortLocationTable(getTable("loca").offset);
    }
    else {
        readLongLocationTable(getTable("loca").offset);
    }

    readGlyphTable(getTable("glyf").offset);

    initWindow(640, 480, "Font Renderer");

    int leftDown = 0;
    int rightDown = 0;
    int move = 250;
    int xPos = -8500;
    int yPos = 350;
    int scale = 5;

    while (1) {
        Event event;
        while(checkWindowEvent(&event)) {
            if (event.type == WINDOW_CLOSE) {
                return 0;
            }
            if (event.type == KEY_CHANGE) {
                if (event.keychange.key == 105) {
                    leftDown = event.keychange.state;
                }
                if (event.keychange.key == 106) {
                    rightDown = event.keychange.state;
                }
            }
        }
        if (leftDown) {
            xPos -= 10;
        }
        if (rightDown) {
            xPos += 10;
        }
        rectangle(0, 0, 640, 480, 0x00000000);
        // draw glyphs
        for (int i = 0; i < numGlyphs; i++) {
            int startIndex = 0;
            for (int j = 0; j < glyphs[i].numContours; j++) {
                for (int k = startIndex; k < glyphs[i].contourEndIndices[j]; k++) {
                    Point a;
                    a.x = glyphs[i].xCoordinates[k]/scale + (move*i) + xPos;
                    a.y = -glyphs[i].yCoordinates[k]/scale + yPos;
                    Point b;
                    b.x = glyphs[i].xCoordinates[k+1]/scale + (move*i) + xPos;
                    b.y = -glyphs[i].yCoordinates[k+1]/scale + yPos;
                    line(a.x, a.y, b.x, b.y, 0x0000ff00);
                }
                Point a;
                a.x = glyphs[i].xCoordinates[glyphs[i].contourEndIndices[j]]/scale + (move*i) + xPos;
                a.y = -glyphs[i].yCoordinates[glyphs[i].contourEndIndices[j]]/scale + yPos;
                Point b;
                b.x = glyphs[i].xCoordinates[startIndex]/scale + (move*i) + xPos;
                b.y = -glyphs[i].yCoordinates[startIndex]/scale + yPos;
                line(a.x, a.y, b.x, b.y, 0x0000ff00);
                for (int k = startIndex; k <= glyphs[i].contourEndIndices[j]; k += 2) {
                    if ((glyphs[i].pointFlags[k] & 0x01) && (glyphs[i].pointFlags[k+1] & 0x01)) {
                        Point a;
                        a.x = glyphs[i].xCoordinates[k]/scale + (move*i) + xPos;
                        a.y = -glyphs[i].yCoordinates[k]/scale + yPos;
                        Point b;
                        b.x = glyphs[i].xCoordinates[k+1]/scale + (move*i) + xPos;
                        b.y = -glyphs[i].yCoordinates[k+1]/scale + yPos;
                        line(a.x, a.y, b.x, b.y, 0xffffffff);
                    }
                }
                startIndex = glyphs[i].contourEndIndices[j] + 1;
            }
        }
        updateWindow();
    }
}