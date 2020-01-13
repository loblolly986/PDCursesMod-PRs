/* PDCurses */

#include <curspriv.h>

/*man-start**************************************************************

initscr
-------

### Synopsis

    WINDOW *initscr(void);
    WINDOW *Xinitscr(int argc, char *argv[]);
    int endwin(void);
    bool isendwin(void);
    SCREEN *newterm(const char *type, FILE *outfd, FILE *infd);
    SCREEN *set_term(SCREEN *new);
    void delscreen(SCREEN *sp);

    int resize_term(int nlines, int ncols);
    bool is_termresized(void);
    const char *curses_version(void);
    void PDC_get_version(PDC_VERSION *ver);

    int set_tabsize(int tabsize);

### Description

   initscr() should be the first curses routine called.  It will
   initialize all curses data structures, and arrange that the
   first call to refresh() will clear the screen.  In case of
   error, initscr() will write a message to standard error and end
   the program.

   endwin() should be called before exiting or escaping from curses
   mode temporarily.  It will restore tty modes, move the cursor to
   the lower left corner of the screen and reset the terminal into
   the proper non-visual mode.  To resume curses after a temporary
   escape, call refresh() or doupdate().

   isendwin() returns TRUE if endwin() has been called without a
   subsequent refresh, unless SP is NULL.

   In some implementations of curses, newterm() allows the use of
   multiple terminals. Here, it's just an alternative interface for
   initscr(). It always returns SP, or NULL.

   delscreen() frees the memory allocated by newterm() or
   initscr(), since it's not freed by endwin(). This function is
   usually not needed. In PDCurses, the parameter must be the
   value of SP, and delscreen() sets SP to NULL.

   set_term() does nothing meaningful in PDCurses, but is included
   for compatibility with other curses implementations.

   resize_term() is effectively two functions: When called with
   nonzero values for nlines and ncols, it attempts to resize the
   screen to the given size. On some platforms,  you can do this
   before calling initscr(), thereby setting the starting screen
   size.  When called with (0, 0), it merely adjusts the internal
   structures to match the current size after the screen is
   resized by the user.  User,  pre-initscr(),  and programmatic
   resizing is platform-dependent,  as follows (with ncurses added
   as a "platform" for comparison;  it supports only user resizing.)

   Platform  User Init Programmatic
   DOS        -     -      x
   ncurses    x     -      -
   OS/2       -     -      x
   SDL1       x     x      x
   SDL2       x     x      x
   VT         x     x      x
   Wincon     x     -      x
   WinGUI     x     x      x
   X11        x     x      -

   If you want to support user resizing, you should check for getch()
   returning KEY_RESIZE, and/or call is_termresized() at appropriate
   times; if either condition occurs, call resize_term(0, 0). Then,
   with either user or programmatic resizing, you'll have to resize
   any windows you've created, as appropriate; resize_term() only
   handles stdscr and curscr.

   is_termresized() returns TRUE if the curses screen has been
   resized by the user, and a call to resize_term() is needed.
   Checking for KEY_RESIZE is generally preferable, unless you're
   not handling the keyboard.

   curses_version() returns a string describing the version of
   PDCurses.

   PDC_get_version() fills a PDC_VERSION structure provided by the user
   with more detailed version info (see curses.h).

   set_tabsize() sets the tab interval, stored in TABSIZE.

### Return Value

   All functions return NULL on error, except endwin(), which always
   returns OK, and resize_term(), which returns either OK or ERR.

### Portability
                             X/Open    BSD    SYS V
    initscr                     Y       Y       Y
    endwin                      Y       Y       Y
    isendwin                    Y       -      3.0
    newterm                     Y       -       Y
    set_term                    Y       -       Y
    delscreen                   Y       -      4.0
    resize_term                 -       -       -
    is_termresized              -       -       -
    curses_version              -       -       -

**man-end****************************************************************/

#include <stdlib.h>

char ttytype[128];

static const char *_curses_notice =
      "PDCurses " PDC_stringize(PDC_VER_MAJOR) "."
                  PDC_stringize(PDC_VER_MINOR) "."
                  PDC_stringize(PDC_VER_CHANGE) " - Public Domain "
                  PDC_stringize(PDC_VER_YEAR) "-"
                  PDC_stringize(PDC_VER_MONTH) "-"
                  PDC_stringize(PDC_VER_DAY);

SCREEN *SP = (SCREEN*)NULL;           /* curses variables */
WINDOW *curscr = (WINDOW *)NULL;      /* the current screen image */
WINDOW *stdscr = (WINDOW *)NULL;      /* the default screen window */
WINDOW *pdc_lastscr = (WINDOW *)NULL; /* the last screen image */

int LINES = 0;                        /* current terminal height */
int COLS = 0;                         /* current terminal width */
int TABSIZE = 8;

MOUSE_STATUS Mouse_status, pdc_mouse_status;

extern RIPPEDOFFLINE linesripped[5];
extern char linesrippedoff;

WINDOW *Xinitscr(int argc, char *argv[])
{
    int i;

    PDC_LOG(("Xinitscr() - called\n"));

    if (SP && SP->alive)
        return NULL;

    if (PDC_scr_open(argc, argv) == ERR)
    {
        fprintf(stderr, "initscr(): Unable to create SP\n");
        exit(8);
    }

    SP->autocr = TRUE;       /* cr -> lf by default */
    SP->raw_out = FALSE;     /* tty I/O modes */
    SP->raw_inp = FALSE;     /* tty I/O modes */
    SP->cbreak = TRUE;
    SP->key_modifiers = 0L;
    SP->return_key_modifiers = FALSE;
    SP->echo = TRUE;
    SP->visibility = 1;
    SP->resized = FALSE;
    SP->_trap_mbe = 0L;
    SP->linesrippedoff = 0;
    SP->linesrippedoffontop = 0;
    SP->delaytenths = 0;
    SP->line_color = -1;

    SP->orig_cursor = PDC_get_cursor_mode();

    LINES = SP->lines;
    COLS = SP->cols;

    if (LINES < 2 || COLS < 2)
    {
        fprintf(stderr, "initscr(): LINES=%d COLS=%d: too small.\n",
                LINES, COLS);
        exit(4);
    }

    if ((curscr = newwin(LINES, COLS, 0, 0)) == (WINDOW *)NULL)
    {
        fprintf(stderr, "initscr(): Unable to create curscr.\n");
        exit(2);
    }

    if ((pdc_lastscr = newwin(LINES, COLS, 0, 0)) == (WINDOW *)NULL)
    {
        fprintf(stderr, "initscr(): Unable to create pdc_lastscr.\n");
        exit(2);
    }

    wattrset(pdc_lastscr, (chtype)(-1));
    werase(pdc_lastscr);

    PDC_slk_initialize();
    LINES -= SP->slklines;

    /* We have to sort out ripped off lines here, and reduce the height
       of stdscr by the number of lines ripped off */

    for (i = 0; i < linesrippedoff; i++)
    {
        if (linesripped[i].line < 0)
            (*linesripped[i].init)(newwin(1, COLS, LINES - 1, 0), COLS);
        else
            (*linesripped[i].init)(newwin(1, COLS,
                                   SP->linesrippedoffontop++, 0), COLS);

        SP->linesrippedoff++;
        LINES--;
    }

    linesrippedoff = 0;

    if (!(stdscr = newwin(LINES, COLS, SP->linesrippedoffontop, 0)))
    {
        fprintf(stderr, "initscr(): Unable to create stdscr.\n");
        exit(1);
    }

    wclrtobot(stdscr);

    /* If preserving the existing screen, don't allow a screen clear */

    if (SP->_preserve)
    {
        untouchwin(curscr);
        untouchwin(stdscr);
        stdscr->_clear = FALSE;
        curscr->_clear = FALSE;
    }
    else
        curscr->_clear = TRUE;

    PDC_init_atrtab();  /* set up default colors */

    MOUSE_X_POS = MOUSE_Y_POS = -1;
    BUTTON_STATUS(1) = BUTTON_RELEASED;
    BUTTON_STATUS(2) = BUTTON_RELEASED;
    BUTTON_STATUS(3) = BUTTON_RELEASED;
    Mouse_status.changes = 0;

    SP->alive = TRUE;

    def_shell_mode();

    longname( );   /* to ensure that ttytype is initialized */

    return stdscr;
}

WINDOW *initscr(void)
{
    PDC_LOG(("initscr() - called\n"));

    return Xinitscr(0, NULL);
}

int endwin(void)
{
    PDC_LOG(("endwin() - called\n"));

    /* Allow temporary exit from curses using endwin() */

    def_prog_mode();
    PDC_scr_close();

    SP->alive = FALSE;

    return OK;
}

bool isendwin(void)
{
    PDC_LOG(("isendwin() - called\n"));

    return SP ? !(SP->alive) : FALSE;
}

SCREEN *newterm(const char *type, FILE *outfd, FILE *infd)
{
    PDC_LOG(("newterm() - called\n"));

    return Xinitscr(0, NULL) ? SP : NULL;
}

SCREEN *set_term(SCREEN *new)
{
    PDC_LOG(("set_term() - called\n"));

    /* We only support one screen */

    return (new == SP) ? SP : NULL;
}

void delscreen(SCREEN *sp)
{
    PDC_LOG(("delscreen() - called\n"));

    if (sp != SP)
        return;

    PDC_slk_free();     /* free the soft label keys, if needed */

    delwin(stdscr);
    delwin(curscr);
    delwin(pdc_lastscr);
    stdscr = (WINDOW *)NULL;
    curscr = (WINDOW *)NULL;
    pdc_lastscr = (WINDOW *)NULL;

    SP->alive = FALSE;

    PDC_scr_free();     /* free SP and pdc_atrtab */

    SP = (SCREEN *)NULL;
}

int resize_term(int nlines, int ncols)
{
    PDC_LOG(("resize_term() - called: nlines %d\n", nlines));

    if( PDC_resize_screen(nlines, ncols) == ERR)
        return ERR;

    if (!stdscr)
        return OK;

    SP->lines = PDC_get_rows();
    LINES = SP->lines - SP->linesrippedoff - SP->slklines;
    SP->cols = COLS = PDC_get_columns();

    if (wresize(curscr, SP->lines, SP->cols) == ERR ||
        wresize(stdscr, LINES, COLS) == ERR ||
        wresize(pdc_lastscr, SP->lines, SP->cols) == ERR)
        return ERR;

    werase(pdc_lastscr);
    curscr->_clear = TRUE;

    if (SP->slk_winptr)
    {
        if (wresize(SP->slk_winptr, SP->slklines, COLS) == ERR)
            return ERR;

        wmove(SP->slk_winptr, 0, 0);
        wclrtobot(SP->slk_winptr);
        PDC_slk_initialize();
        slk_noutrefresh();
    }

    touchwin(stdscr);
    wnoutrefresh(stdscr);
    return OK;
}

bool is_termresized(void)
{
    PDC_LOG(("is_termresized() - called\n"));

    return SP->resized;
}

const char *curses_version(void)
{
    return _curses_notice;
}

void PDC_get_version(PDC_VERSION *ver)
{
    extern enum PDC_port PDC_port_val;

    if (!ver)
        return;

    ver->flags = 0
#ifdef PDCDEBUG
        | PDC_VFLAG_DEBUG
#endif
#ifdef PDC_WIDE
        | PDC_VFLAG_WIDE
#endif
#ifdef PDC_FORCE_UTF8
        | PDC_VFLAG_UTF8
#endif
#ifdef PDC_DLL_BUILD
        | PDC_VFLAG_DLL
#endif
#ifdef PDC_RGB
        | PDC_VFLAG_RGB
#endif
        ;

    ver->build = PDC_BUILD;
    ver->major = PDC_VER_MAJOR;
    ver->minor = PDC_VER_MINOR;
    ver->change = PDC_VER_CHANGE;
    ver->csize = sizeof(chtype);
    ver->bsize = sizeof(bool);
    ver->port = PDC_port_val;
}

int set_tabsize(int tabsize)
{
    PDC_LOG(("set_tabsize() - called: tabsize %d\n", tabsize));

    if (tabsize < 1)
        return ERR;

    TABSIZE = tabsize;

    return OK;
}
