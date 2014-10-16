// ConsoleGL
//
// Licensed public domain

/**
 *  Opens a window
 */
const char* CGLmain(const char* _windowTitle, int _columns, int _rows, void (*_callback)(void));

/**
 *  Sets the default color attribute used.
 *  bits 0-3 = FG Color
 *  bits 4-6 = BG Color
 *  bit 7 = Blink
 */
void CGLsetAttrib(int _attrib);

/**
 * Sets the attribute at the specified location
 */
void CGLsetAttribXY(int _attrib, int _col, int _row);

/**
 * Moves the cursor to the specified location
 */
void CGLgotoXY(int _col, int _row);

/**
 * Print a formatted string
 */
void CGLprintf(const char * _format, ...);

/**
 * Prints a single char to the screen
 */
void CGLputc(char _char);

/**
 * Closes the window and cleans up
 */
void CGLshutdown();
