/* clock.c */
/* tell our linker clock_isr is not in this file */
extern void clock_isr(void);

__volatile__ unsigned long clock_count=0L;

void ISR_clock(void)
{
    clock_count++;
}
