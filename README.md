# Font Parser / Renderer
A program to read a TrueType font file and draw its glyphs as outlines. Use left and right arrow to move the text, press C to show control contours (off-curve points). Made with xgfx.

Tested with Droid Sans Mono and Ubuntu Condensed.

## Issues
- Doesn't support compound glyphs, replaces them with Unknown Glyph glyph.
- Doesn't read `cmap`, so can't render text strings (yet)
- For some reason breaks when I try to load all the glyphs, so it only loads the first 300.