/*
 * call.h
 *
 * Created: 14.02.2014 23:19:50
 *  Author: rballis
 */ 


#ifndef DVSET_H_
#define DVSET_H_

#define DVSET_LINE_LENGTH 22

void dvset_field(int act);
void dvset_cursor(int act);
void dvset_select(bool select);
void dvset_cancel(void);
void dvset_clear(void);
void dvset_store(void);
void dvset_goedit(void);
void dvset_backspace(void);
bool dvset_isselected(void);
bool dvset_isedit(void);
void dvset(void);
void dvset_print(void);


#endif /* DVSET_H_ */