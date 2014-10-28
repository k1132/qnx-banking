#ifndef SIM_H_
#define SIM_H_

#include <unistd.h>

#define NSC_PER_SIM_SEC ((long )(100000000/60))
#define SIM_SEC_PER_SEC ((long )(600))

#define SIM_MIL_TO_SEC(HR, MI) ((3600 * HR) + (60 * MI))
#define SIM_SEC_TO_NSC(SEC) ((NSC_PER_SIM_SEC * SEC))
#define NSC_TO_SIM_SEC(NSC) ((NSC / NSC_PER_SIM_SEC))
#define SEC_TO_SIM_SEC(SEC) ((SEC * SIM_SEC_PER_SEC))

#define MIN_TO_SEC(MI) (60 * MI)

#define SIM_CHOOSE_HI_EXCLUSIVE 0
int sim_choose(unsigned int *seed, unsigned int lo, unsigned int hi);

void sim_sleep(int sim_seconds, int *sim_sec);

void sim_elaps_init(struct timespec *t0);
void sim_elaps_calc(struct timespec *t0, int *sim_sec);

void sim_fmt_time(char* thd_buf, size_t count, int sim_s);

/*
unsigned int sim_mil_to_min(unsigned int hr, unsigned int mi);
useconds_t sim_min_to_mic(unsigned int minutes);
unsigned int mic_to_sim_min(useconds_t micros);
*/

#endif
