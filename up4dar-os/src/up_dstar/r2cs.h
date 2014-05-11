/*
 * r2cs.h
 *
 * Created: 04.03.2014 08:37:32
 *  Author: rballis
 */ 


#ifndef R2CS_H_
#define R2CS_H_

#define  R2CS_HISTORY_DIM 4

void r2cs(int layer, int position);
bool r2csURCALL(void);
int r2cs_count(void);
int r2cs_position(void);
char* r2cs_get(int position);
void r2cs_append(const char urcall[8]);
void r2cs_set_orig(void);
void r2cs_print(int layer, int position);


#endif /* R2CS_H_ */