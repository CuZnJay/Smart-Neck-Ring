#ifndef __BREAKAGE_ALARM_H
#define __BREAKAGE_ALARM_H

#define Breakage_Scan_PIN               56  /* PD8 */
#define Breakage_Scan_Reset_Button_Pin  67  /* PE3 */

extern uint8_t Ctrl_Z;

void Breakage_Alarm_Start(void);

#endif
