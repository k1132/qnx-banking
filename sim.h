#ifndef SIM_H_
#define SIM_H_

#include <unistd.h>

#define MIC_PER_SIM_SEC ((useconds_t )1667)

#define SIM_MIL_TO_SEC(HR, MI) ((360 * HR) + (60 * MI))
#define SIM_SEC_TO_MIC(SEC) ((MIC_PER_SIM_SEC * SEC))
#define MIC_TO_SIM_SEC(MIC) ((MIC / MIC_PER_SIM_SEC))

#define MIN_TO_SEC(MI) (60 * MI)

#define SIM_CHOOSE_HI_EXCLUSIVE 0
unsigned int sim_choose(unsigned int lo, unsigned int hi);

/*
unsigned int sim_mil_to_min(unsigned int hr, unsigned int mi);
useconds_t sim_min_to_mic(unsigned int minutes);
unsigned int mic_to_sim_min(useconds_t micros);
*/

#endif
