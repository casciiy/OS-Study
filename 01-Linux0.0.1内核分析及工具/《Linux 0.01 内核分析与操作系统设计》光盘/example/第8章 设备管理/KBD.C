/*****************************************************************************
	kbd.c - PC keyboard demo code/driver for OS
	Christopher Giese <geezer[AT]execpc.com>

	Release date 9/2/99. Distribute freely. ABSOLUTELY NO WARRANTY.
	Compile with DJGPP or Turbo C or possibly Borland C.

Changes/fixes:
	- got numeric keypad and NumLock working
	- slightly different scancode-to-ASCII conversion logic
	- created #defines for raw scancode values
	- different ordering of events during keyboard and controller init
Sources:
	- PORTS.A of Ralf Brown's interrupt list collection
	- repairfaq.org keyboard FAQ at
	    http://www.repairfaq.org/filipg/LINK/PORTS/F_Keyboard_FAQ.html
	- Linux source code
Test hardware:
	- New Samsung KB3T001SAXAA 104-key keyboard
	- Old Maxi 2186035-00-21 101-key keyboard
To do:
	- turn off auto-repeat for lock keys?

	- ioctl()s (functions, for now -- use interrupts?) to
	  - set make/break or make-only for each key
	  - set typematic mode or not for each key
	  - set typematic rate and delay

	- Unicode/UTF-8
	- Alt+number entered on numeric keypad == scancode, like DOS?
*****************************************************************************/
#include	<stdio.h>	/* printf() */
/* union REGS, int86(), inportb(), outportb(), getvect(), setvect() */
#include	<dos.h>

#if 1
#define	DEBUG(X)	X
#else
#define	DEBUG(X)
#endif

/* quick and dirty begin/end critical section... */
#define	critb()		(disable(), 0)
#define	crite(X)	enable()

/* geezer's Portable Interrupt Macros (tm) */
#if defined(__TURBOC__)
/* getvect(), setvect() in dos.h */

/* from Turbo C++ getvect() help */
#ifdef	__cplusplus
#define	__CPPARGS	...
#else
#define	__CPPARGS
#endif

typedef void interrupt(*vector)(__CPPARGS);
#define SAVE_VECT(Num,Vec)	Vec=getvect(Num)
#define	SET_VECT(Num,Fn)	setvect(Num, Fn)
#define	RESTORE_VECT(Num,Vec)	setvect(Num, Vec)

#elif defined(__DJGPP__)
#include	<dpmi.h>	/* _go32_dpmi_... */
#include	<go32.h>	/* _my_cs() */

#define	interrupt
#define	__CPPARGS
typedef _go32_dpmi_seginfo vector;
#define	SAVE_VECT(Num,Vec)	\
	_go32_dpmi_get_protected_mode_interrupt_vector(Num, &Vec)
#define	SET_VECT(Num,Fn)					\
	{	_go32_dpmi_seginfo NewVector;			\
								\
		NewVector.pm_selector=_my_cs();			\
		NewVector.pm_offset=(unsigned long)Fn;		\
		_go32_dpmi_allocate_iret_wrapper(&NewVector);	\
		_go32_dpmi_set_protected_mode_interrupt_vector	\
			(Num, &NewVector); }
#define	RESTORE_VECT(Num,Vec)	\
	_go32_dpmi_set_protected_mode_interrupt_vector(Num, &Vec)

#else
#error Not Turbo C nor DJGPP, sorry.
#endif

/* Info from file PORTS.A of Ralf Brown's interrupt list collection
   I/O addresses of 8042 keyboard controller on PC motherboard */
#define	_KBD_STAT_REG		0x64
#define	_KBD_CMD_REG		0x64
#define	_KBD_DATA_REG		0x60

/* 8042 controller commands (write to 0x64; Table P0401 in PORTS.A) */
#define	_KBD_CCMD_WRCMDB	0x60	/* write "Command Byte" */
#define	_KBD_CCMD_TEST1		0xAA	/* controller self-test */
#define	_KBD_CCMD_TEST2		0xAB	/* interface test */
#define	_KBD_CCMD_ENABLE	0xAE
/* Linux enables A20 by writing _KBD_CCMD_WROPRT to 0x64 and 0xDF to 0x60 */
#define	_KBD_CCMD_WROPRT	0xD1	/* write 8042 output port */

/* keyboard commands (write to 0x60; Table P0386 in PORTS.A) */
#define	_KBD_KCMD_LEDS		0xED	/* set LEDs */
#define	_KBD_KCMD_SCSET		0xF0	/* get/set scancode set */
#define	_KBD_KCMD_TYPEM		0xF3	/* set auto-repeat settings */
#define	_KBD_KCMD_EN		0xF4	/* enable scanning */
#define	_KBD_KCMD_DIS		0xF5	/* disable scanning */
#define	_KBD_KCMD_TMB		0xFA	/* all keys typematic/make-break */
#define	_KBD_KCMD_RESET		0xFF	/* full reset of 8048 + self-test */

/* bits for "Command Byte" (confusing name; Table P0404 in PORTS.A).
Bit names are from Linux source.
				0x80	reserved */
#define	_KBD_CB_KCC		0x40	/* do AT-to-XT scancode translation */
#define	_KBD_CB_DMS		0x20	/* force mouse clock low */
/*				0x10	disable keyboard clock (?)
				0x08	ignore keyboard lock switch (reserved?) */
#define	_KBD_CB_SYS		0x04	/* system flag (?) */
/*				0x02	reserved (enable PS/2 mouse IRQ12?) */
#define	_KBD_CB_EKI		0x01	/* enable IRQ1 for _KBD_STAT_OBF */
/* Linux also sets _KBD_CB_KCC here: */
#define	_KBD_CB_INIT		(_KBD_CB_DMS | _KBD_CB_SYS | _KBD_CB_EKI)


/* status bits read from 0x64 (Table P0398 in PORTS.A) */
#define	_KBD_STAT_PERR		0x80	/* parity error in data from kbd */
#define	_KBD_STAT_GTO		0x40	/* receive timeout */
/*				0x20	xmit timeout
				0x10	keyboard locked
				0x08
	=1 data written to input register is command (PORT 0064h)
	=0 data written to input register is data (PORT 0060h)
				0x04
	system flag status: 0=power up or reset	 1=self-test OK */
#define	_KBD_STAT_IBF		0x02	/* input buffer full (data for kbd) */
#define	_KBD_STAT_OBF		0x01	/* output bffr full (data for host) */

/* #defines from here down can be brought out to a separate .h file,
for use by programs that talk to the keyboard */

/* "bucky bits" */
#define	KBD_BUCKY_ALT		0x8000	/* Alt is pressed */
#define	KBD_BUCKY_CTRL		0x4000	/* Ctrl is pressed */
#define	KBD_BUCKY_SHIFT		0x2000	/* Shift is pressed */
#define	KBD_BUCKY_ANY		\
			(KBD_BUCKY_ALT | KBD_BUCKY_CTRL | KBD_BUCKY_SHIFT)
#define	KBD_BUCKY_CAPS		0x1000	/* CapsLock is on */
#define	KBD_BUCKY_NUM		0x0800	/* NumLock is on */
#define	KBD_BUCKY_SCRL		0x0400	/* ScrollLock is on */

/* "raw" set 3 scancodes */
#define	RK_LEFT_CTRL	0x11
#define	RK_LEFT_SHIFT	0x12
#define	RK_CAPS_LOCK	0x14
#define	RK_LEFT_ALT	0x19
#define	RK_RIGHT_ALT	0x39
#define	RK_RIGHT_CTRL	0x58
#define	RK_RIGHT_SHIFT	0x59
#define	RK_SCROLL_LOCK	0x5F
#define	RK_NUM_LOCK	0x76
#define	RK_END_1	0x69	/* End/1 on numeric keypad */
#define	RK_BREAK_CODE	0xF0

/* "ASCII" values for non-ASCII keys. All of these are user-defined.
Function keys: */
#define	K_F1		0x100
#define	K_F2		(K_F1 + 1)
#define	K_F3		(K_F2 + 1)
#define	K_F4		(K_F3 + 1)
#define	K_F5		(K_F4 + 1)
#define	K_F6		(K_F5 + 1)
#define	K_F7		(K_F6 + 1)
#define	K_F8		(K_F7 + 1)
#define	K_F9		(K_F8 + 1)
#define	K_F10		(K_F9 + 1)
#define	K_F11		(K_F10 + 1)
#define	K_F12		(K_F11 + 1)	/* 0x10B */
/* cursor keys */
#define	K_INS		(K_F12 + 1)	/* 0x10C */
#define	K_DEL		(K_INS + 1)
#define	K_HOME		(K_DEL + 1)
#define	K_END		(K_HOME + 1)
#define	K_PGUP		(K_END + 1)
#define	K_PGDN		(K_PGUP + 1)
#define	K_LFT		(K_PGDN + 1)
#define	K_UP		(K_LFT + 1)
#define	K_DN		(K_UP + 1)
#define	K_RT		(K_DN + 1)	/* 0x115 */
/* print screen/sys rq and pause/break */
#define	K_PRNT		(K_RT + 1)	/* 0x116 */
#define	K_PAUSE		(K_PRNT + 1)	/* 0x117 */
/* these return a value but they could also act as additional bucky keys */
#define	K_LWIN		(K_PAUSE + 1)	/* 0x118 */
#define	K_RWIN		(K_LWIN + 1)
#define	K_MENU		(K_RWIN + 1)	/* 0x11A */

#if defined(__TURBOC__) || !defined(__cplusplus)
typedef enum
{	false=0, true=1 } bool;
#endif

typedef unsigned char	u8;
typedef unsigned short	u16;

typedef struct	/* circular queue */
{	bool NonEmpty;
	u8 *Data;
	u16 Size, Inptr, Outptr; } queue;

typedef struct	/* virtual console! */
{	queue Keystrokes;
	u16 KbdStatus; } console;

static console _Con;
/*****************************************************************************
	name:	inq
	action:	tries to add byte Data to queue Queue
	returns:-1 queue full
		0  success
*****************************************************************************/
int inq(queue *Queue, u8 Data)
{	u16 Temp;

	Temp=Queue->Inptr + 1;
	if(Temp >= Queue->Size)
		Temp=0;
	if(Temp == Queue->Outptr)
		return(-1);	/* full */
	Queue->Data[Queue->Inptr]=Data;
	Queue->Inptr=Temp;
	Queue->NonEmpty=true;
	return(0); }
/*****************************************************************************
	name:	deq
	action:	tries to get byte from Queue
	returns:-1  queue empty
		>=0 success (byte read)
*****************************************************************************/
int deq(queue *Queue)
{	u8 RetVal;

	if(Queue->NonEmpty == false)
		return(-1);	/* empty */
	RetVal=Queue->Data[Queue->Outptr++];
	if(Queue->Outptr >= Queue->Size)
		Queue->Outptr=0;
	if(Queue->Outptr == Queue->Inptr)
		Queue->NonEmpty=false;
	return(RetVal); }
/*****************************************************************************
	name:	kbdRead
	action:	waits for a byte from the keyboard
	returns:-1 if timeout
		nonnegative byte if success
*****************************************************************************/
static int _kbdRead(void)
{	unsigned long Timeout;
	u8 Stat, Data;

	for(Timeout=500000L; Timeout != 0; Timeout--)
	{	Stat=inportb(_KBD_STAT_REG);
/* loop until 8042 output buffer full */
		if((Stat & _KBD_STAT_OBF) != 0)
		{	Data=inportb(_KBD_DATA_REG);
/* loop if parity error or receive timeout */
			if((Stat & (_KBD_STAT_PERR | _KBD_STAT_GTO)) == 0)
				return(Data); }}
	return(-1); }
/*****************************************************************************
	name:	kbdWrite
	action:	writes Data to 8048 keyboard MCU (Adr=_KBD_DATA_REG) or
		8042 keyboard controller (Adr=_KBD_CMD_REG)
*****************************************************************************/
static void _kbdWrite(unsigned Adr, unsigned Data)
{	unsigned long Timeout;
	u8 Stat;

/* Linux code didn't have a timeout here... */
	for(Timeout=500000L; Timeout != 0; Timeout--)
	{	Stat=inportb(_KBD_STAT_REG);
/* loop until 8042 input buffer empty */
		if((Stat & _KBD_STAT_IBF) == 0)
			break; }
	if(Timeout != 0)
		outportb(Adr, Data);
/* xxx - else? */ }
/*****************************************************************************
	name:	kbdWriteRead
	action:	writes value Val to keyboard port Adr. If Len is
		non-zero, reads Len bytes from keyboard and compares
		them to Reply
	returns:0 if success
		nonzero if error (returns value of failing reply byte)
*****************************************************************************/
int _kbdWriteRead(unsigned Adr, unsigned Val, unsigned Len, u8 *Reply)
{	int RetVal;

	_kbdWrite(Adr, Val);
	for(; Len != 0; Len--)
	{	RetVal=_kbdRead();
		if(*Reply != RetVal)
		{	printf(" failed: expected 0x%02X, got 0x%02X\n",
				*Reply, RetVal);
			return(RetVal); }
		Reply++; }
	return(0); }
/*****************************************************************************
	name:	kbdInit
	action:	runs self-test on 8048 keyboard MCU and 8042 controller MCU,
		resets keyboard, programs 8048 for ScanCodeSet,
		programs 8042 for no XT-to-AT conversion,
		makes all keys make/break and fast typematic
	returns:0 if success
		nonzero if error
*****************************************************************************/
int kbdInit(unsigned ScanCodeSet)
{	unsigned Flags;
	int Result;

	printf("keyboard:");
/* xxx - probe for keyboard controller before proceding?
I don't know what will happen if the controller is bad or missing...

disable interrupts */
	Flags=critb();
/* flush pending input */
	DEBUG(printf("\n  flushing keyboard output");)
	while(_kbdRead() != -1)
		/* nothing */;
/* test controller, expect 0x00 if successful */
	DEBUG(printf("\n  controller self-test");)
	Result=_kbdWriteRead(_KBD_CMD_REG, _KBD_CCMD_TEST2, 1,
		(u8 *)"\x00");
	if(Result != 0)
	{	static const char *Line[2]={ "clock", "data" };
		static const char *Stuck[2]={ "low", "high" };

		printf("  *** interface test failed, ");
		Result--;
		if(Result > 3)
			printf("cause unknown\n");
		else printf("%s line stuck %s\n", Line[Result >> 1],
			Stuck[Result & 1]);
BAIL:		crite(Flags);	/* restore previous interrupt mask */
		return(-1); }
/* enable keyboard interface on controller. Controller commands
(_KBD_CCMD_nnn) may return a data byte, but do not return an ACK */
	_kbdWrite(_KBD_CMD_REG, _KBD_CCMD_ENABLE);
/* reset keyboard (selects scancode set 2), expect 0xFA and 0xAA */
	DEBUG(printf("\n  resetting keyboard");)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_RESET, 2,
		(u8 *)"\xFA\xAA");
	if(Result != 0)
		goto BAIL;
/* test keyboard, expect 0x55 if successful */
	DEBUG(printf("\n  keyboard self-test");)
	Result=_kbdWriteRead(_KBD_CMD_REG, _KBD_CCMD_TEST1, 1,
		(u8 *)"\x55");
	if(Result != 0)
		goto BAIL;
/* disable keyboard before programming it, expect 0xFA (ACK) */
	DEBUG(printf("\n  disable");)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_DIS, 1,
		(u8 *)"\xFA");
	if(Result != 0)
		goto BAIL;
/* disable AT-to-XT keystroke conversion, disable PS/2 mouse,
set SYS bit, and Enable Keyboard Interrupt */
	_kbdWrite(_KBD_CMD_REG, _KBD_CCMD_WRCMDB);
	_kbdWrite(_KBD_DATA_REG, _KBD_CB_INIT);	/* DMS, SYS, EKI */
/* program desired scancode set, expect 0xFA (ACK)... */
	DEBUG(printf("\n  programming scancode set %u", ScanCodeSet);)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_SCSET, 1,
		(u8 *)"\xFA");
	if(Result != 0)
		goto BAIL;
/* ...send scancode set value, expect 0xFA (ACK) */
	Result=_kbdWriteRead(_KBD_DATA_REG, ScanCodeSet, 1,
		(u8 *)"\xFA");
	if(Result != 0)
		goto BAIL;
/* make all keys typematic (auto-repeat) and make-break. This may work
only with scancode set 3, I'm not sure. Expect 0xFA (ACK) */
	DEBUG(printf("\n  making all keys typematic and make-break");)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_TMB, 1,
		(u8 *)"\xFA");

	// -----------------------------
	//  if(Result != 0)  goto BAIL;

/* set typematic delay as short as possible;
repeat as fast as possible, expect ACK... */
	DEBUG(printf("\n  setting fast typematic mode");)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_TYPEM, 1,
		(u8 *)"\xFA");
	
	// ----------------------------
	//if(Result != 0) goto BAIL;

/* ...typematic control byte (0 corresponds to MODE CON RATE=30 DELAY=1),
expect 0xFA (ACK) */
	Result=_kbdWriteRead(_KBD_DATA_REG, 0, 1,
		(u8 *)"\xFA");
	if(Result != 0)
		goto BAIL;
/* enable keyboard, expect 0xFA (ACK) */
	DEBUG(printf("\n  enable");)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_EN, 1,
		(u8 *)"\xFA");
	if(Result != 0)
		goto BAIL;
	printf("\n  scancode set 3 driver activated\n");
/* restore previous interrupt mask */
	crite(Flags);
	return(0); }
/*****************************************************************************
	name:	irq1
	action:	IRQ1 (INT 9) interrupt handler (keyboard);
		puts scancode in Keystrokes queue (unless it's full)
*****************************************************************************/
static void interrupt _irq1(__CPPARGS)
{	u8 Code;

	Code=inportb(_KBD_DATA_REG);
	if(inq(&(_Con.Keystrokes), Code) != 0)
		/* full queue, beep or something */;
	outportb(0x20, 0x20); }	/* send EOI to 8259 interrupt controller */
/*****************************************************************************
	name:	my_getch
	action:	waits for keypress, converts raw set 3 scancodes to
		extended ASCII
	returns:extended ASCII value
*****************************************************************************/
int my_getch(console *_Con)
{/* convert raw set 3 scancodes to ASCII, no Shift
A-Z and a-z are in the "wrong" maps (this slightly simplifies the
conversion logic for Ctrl), but we fix that below */
	static const u16 Map[]={
/* 00 */0,	0,	0,	0,	0,	0,	0,	K_F1,
/* 08 */0x1B,	0,	0,	0,	0,	0x09,	'`',	K_F2,
/* 11 is left Ctrl; 12 is left Shift; 14 is CapsLock */
/* 10 */0,	0,	0,	0,	0,	'Q',	'1',	K_F3,
/* 19 is left Alt */
/* 18 */0,	0,	'Z',	'S',	'A',	'W',	'2',	K_F4,
/* 20 */0,	'C',	'X',	'D',	'E',	'4',	'3',	K_F5,
/* 28 */0,	' ',	'V',	'F',	'T',	'R',	'5',	K_F6,
/* 30 */0,	'N',	'B',	'H',	'G',	'Y',	'6',	K_F7,
/* 39 is right Alt */
/* 38 */0,	0,	'M',	'J',	'U',	'7',	'8',	K_F8,
/* 40 */0,	',',	'K',	'I',	'O',	'0',	'9',	K_F9,
/* 48 */0,	'.',	'/',	'L',	';',	'P',	'-',	K_F10,
/* 50 */0,	0,	'\'',	0,	'[',	'=',	K_F11,	K_PRNT,
/* 58 is right Ctrl; 59 is right Shift; 5F is Scroll Lock */
/* 58 */0,	0,	0x0D,	']',	'\\',	0,	K_F12,	0,
/* 60 */K_DN,	K_LFT,	K_PAUSE,K_UP,	K_DEL,	K_END,	0x08,	K_INS,
/* 68 */0,	K_END,	K_RT,	K_LFT,	K_HOME,	K_PGDN,	K_HOME,	K_PGUP,
/* 76 is Num Lock */
/* 70 */K_INS,	K_DEL,	K_DN,	'5',	K_RT,	K_UP,	0,	'/',
/* 78 */0,	0x0D,	K_PGDN,	0,	'+',	K_PGUP,	'*',	0,
/* 80 */0,	0,	0,	0,	'-',	0,	0,	0,
/* 88 */0,	0,	0,	K_LWIN,	K_RWIN,	K_MENU,	0,	0 };
/* convert raw set 3 scancodes to ASCII with Shift */
	static const u16 ShiftMap[]={
/* 00 */0,	0,	0,	0,	0,	0,	0,	K_F1,
/* 08 */0x1B,	0,	0,	0,	0,	0x09,	'~',	K_F2,
/* 10 */0,	0,	0,	0,	0,	'q',	'!',	K_F3,
/* 18 */0,	0,	'z',	's',	'a',	'w',	'@',	K_F4,
/* 20 */0,	'c',	'x',	'd',	'e',	'$',	'#',	K_F5,
/* 28 */0,	' ',	'v',	'f',	't',	'r',	'%',	K_F6,
/* 30 */0,	'n',	'b',	'h',	'g',	'y',	'^',	K_F7,
/* 38 */0,	0,	'm',	'j',	'u',	'&',	'*',	K_F8,
/* 40 */0,	'<',	'k',	'i',	'o',	')',	'(',	K_F9,
/* 48 */0,	'>',	'?',	'l',	':',	'p',	'_',	K_F10,
/* 50 */0,	0,	'"',	0,	'{',	'+',	K_F11,	K_PRNT,
/* 58 */0,	0,	0x0D,	'}',	'|',	0,	K_F12,	0,
/* 60 */K_DN,	K_LFT,	K_PAUSE,K_UP,	K_DEL,	K_END,	0x08,	K_INS,
/* 68 */0,	'1',	K_RT,	'4',	'7',	K_PGDN,	K_HOME,	K_PGUP,
/* 70 */'0',	'.',	'2',	'5',	'6',	'8',	0,	'/',
/* 78 */0,	0x0D,	'3',	0,	'+',	'9',	'*',	0,
/* 80 */0,	0,	0,	0,	'-',	0,	0,	0,
/* 88 */0,	0,	0,	K_LWIN,	K_RWIN,	K_MENU,	0,	0 };

	while(1)
	{	short Code, RetVal;
		bool SawBreakCode;

		SawBreakCode=false;
		do
/* get scancode
xxx - should yield to OS if no scancode available */
		{	do Code=deq(&(_Con->Keystrokes));
			while(Code < 0);
/* step 0: raw set 3 scancodes */
			if(Code == 0xF0)
				SawBreakCode=true;}
		while(Code == 0xF0);
		if(SawBreakCode)
			Code=-Code;
/* step 1: raw scancodes, negated if break code. Good for e.g. video games
Alt pressed = fire weapon, Alt released = cease fire

set "bucky bits" (Alt, Ctrl, and Shift) */
		if(Code == RK_LEFT_ALT || Code == RK_RIGHT_ALT)
		{	_Con->KbdStatus |= KBD_BUCKY_ALT;
			continue; }
		if(Code == -RK_LEFT_ALT || Code == -RK_RIGHT_ALT)
		{	_Con->KbdStatus &= ~KBD_BUCKY_ALT;
			continue; }
		if(Code == RK_LEFT_CTRL || Code == RK_RIGHT_CTRL)
		{	_Con->KbdStatus |= KBD_BUCKY_CTRL;
			continue; }
		if(Code == -RK_LEFT_CTRL || Code == -RK_RIGHT_CTRL)
		{	_Con->KbdStatus &= ~KBD_BUCKY_CTRL;
			continue; }
		if(Code == RK_LEFT_SHIFT || Code == RK_RIGHT_SHIFT)
		{	_Con->KbdStatus |= KBD_BUCKY_SHIFT;
			continue; }
		if(Code == -RK_LEFT_SHIFT || Code == -RK_RIGHT_SHIFT)
		{	_Con->KbdStatus &= ~KBD_BUCKY_SHIFT;
			continue; }
/* not interested in break codes other than Ctrl, Alt, Shift */
		if(Code < 0)
			continue;
/* Scroll Lock, Num Lock, and Caps Lock set the LEDs */
		if(Code == RK_SCROLL_LOCK)
		{	_Con->KbdStatus ^= KBD_BUCKY_SCRL;
			goto LEDS; }
		if(Code == RK_NUM_LOCK)
		{	_Con->KbdStatus ^= KBD_BUCKY_NUM;
			goto LEDS; }
		if(Code == RK_CAPS_LOCK)
		{	_Con->KbdStatus ^= KBD_BUCKY_CAPS;
LEDS:			_kbdWrite(_KBD_DATA_REG, _KBD_KCMD_LEDS);
/* clever choice of KBD_BUCKY_SCRL, etc. makes this work */
			_kbdWrite(_KBD_DATA_REG,
				(_Con->KbdStatus >> 10) & 7);
			continue; }
/* ignore invalid scan codes */
		if(Code >= 0x90)
			continue;
		RetVal=Map[Code];
/* defective keyboard? more than 104 keys? */
		if(RetVal == 0)
			continue;
/* merge bucky bits with raw scan code
(no, don't do this, we still need Code)
//		Code |= (_Con->KbdStatus & KBD_BUCKY_ANY);
/* step 2: raw scancodes with bucky bits. Good for e.g. user-interface
Alt+BackSpace = undo, Shift+Alt+BackSpace = redo

Using tables for scancode-to-ASCII conversion would be more flexible,
but take more memory. With 5 bucky bits (Ctrl, Shift, Alt, NumLock, and
CapsLock), it would take 32 tables, 144 entries each (one entry for
each scancode), 2 bytes/entry == 9216 bytes.

Alt pressed? there is no ASCII equivalent
just return mapped scancode with bucky bits */
		if((_Con->KbdStatus & KBD_BUCKY_ALT) != 0)
			RetVal |= (_Con->KbdStatus & KBD_BUCKY_ANY);
/* Ctrl pressed... */
		else if((_Con->KbdStatus & KBD_BUCKY_CTRL) != 0)
/* ...Ctrl @A-Z[\]^_ return 0-31 */
		{	if(RetVal >= '@' && RetVal <= '_')
				RetVal=RetVal - '@';
/* ...anything else: return mapped scancode with bucky bits */
			else RetVal |= (_Con->KbdStatus & KBD_BUCKY_ANY); }
/* Shift pressed... */
		else if((_Con->KbdStatus & KBD_BUCKY_SHIFT) != 0)
/* ...ASCII: use alternate map to convert Code to RetVal */
		{	if(RetVal >= ' ' && RetVal <= '~')
				RetVal=ShiftMap[Code];
/* ...non-ASCII: return mapped scancode with bucky bits */
			else RetVal |= (_Con->KbdStatus & KBD_BUCKY_ANY); }
/* no Alt, no Ctrl, no Shift: get return value from Map[] unless
	1. NumLock is on
	2. the key pressed is on the numeric keypad
in which case, return value comes from ShiftMap[] instead. Got that? */
		else if(((_Con->KbdStatus & KBD_BUCKY_NUM) != 0)
			&& RetVal >= RK_END_1)
				RetVal=ShiftMap[Code];
/* somehow, after all this, we got an invalid key... */
		if(RetVal == 0)
			continue;
/* CapsLock affects only A-Z
could use tolower() and toupper() here if you want to drag in ctype.h
Logic is backwards to get proper A-Z value from Map[] or ShiftMap[] */
		if((_Con->KbdStatus & KBD_BUCKY_CAPS) == 0)
		{	if(RetVal >= 'a' && RetVal <= 'z')
				RetVal=RetVal - 'a' + 'A';
			else if(RetVal >= 'A' && RetVal <= 'Z')
				RetVal=RetVal - 'A' + 'a'; }
/* ASCII at last... */
		return(RetVal); }}
/*****************************************************************************
	name:	main
*****************************************************************************/
#define	BUF_SIZE	512

int main(void)
{	u8 Buffer[BUF_SIZE];
	int Result, Temp;
	union REGS Regs;
	vector Vector9;

/* make sure it's really DOS */
	Regs.x.ax=0x1600;
	int86(0x2F, &Regs, &Regs);
	if(Regs.h.al != 0 && Regs.h.al != 0x80)
	{	printf("Detected Windows version ");
		if(Regs.h.al == 0x01 || Regs.h.al == 0xFF)
			printf("2.x");
		else printf("%u.%u\n", Regs.h.al, Regs.h.ah);
		printf("This keyboard driver/demo code will not work "
			"under Windows\n");
		return(-1); }
/* try the scancode set 3 keyboard driver */
	if(kbdInit(3) != 0)
		return(2);
/* init Con */
	_Con.Keystrokes.Data=Buffer;
	_Con.Keystrokes.Size=BUF_SIZE;
/* set new IRQ 9 (keyboard) interrupt handler */
	SAVE_VECT(9, Vector9);
	SET_VECT(9, _irq1);
/* demo */
	printf("(Press Esc to quit this demo)\n");
	do
	{	
		Temp=my_getch(&_Con);

		if(Temp < 0x100)
			putchar(Temp);
		else
			printf("<%04X>", Temp);

		printf("key = <%04X>\n", Temp);
		fflush(stdout);


	}while(Temp != '\x1B');

/* restore original keyboard interrupt vector */
	RESTORE_VECT(9, Vector9);
/* go back to scancode set 1 or 2 */
	Temp=1;
	DEBUG(printf("programming scancode set %u\n", Temp);)
	Result=_kbdWriteRead(_KBD_DATA_REG, _KBD_KCMD_SCSET, 1,
		(u8 *)"\xFA");
	if(Result == 0)
/* ...send scancode set value, expect 0xFA (ACK) */
	{	Result=_kbdWriteRead(_KBD_DATA_REG, Temp, 1,
			(u8 *)"\xFA"); }
	return(0); }
