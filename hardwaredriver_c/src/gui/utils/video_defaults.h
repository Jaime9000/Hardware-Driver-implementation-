#ifndef VIDEO_DEFAULTS_H
#define VIDEO_DEFAULTS_H

#include <stdint.h>

typedef struct {
    uint32_t id;
    const char* name;
} VideoMapping;

#define VIDEO_MAPPING_COUNT 41

static const VideoMapping DEFAULT_VIDEO_MAPPINGS[VIDEO_MAPPING_COUNT] = {
    {32834, NULL},
    {32838, NULL},
    {33337, NULL},
    {33346, "Scan 16-20"},
    {33344, NULL},
    {33347, "What Is Nmd-34"},
    {33348, "Scan 18-21"},
    {33345, "Scan 15-19"},
    {33343, "Scan 13-18"},
    {33342, "Scan 12-17"},
    {33341, "Scan 11-16"},
    {32818, "Open_Close-13"},
    {32819, "Joint Open   Close-5"},
    {32820, "Lateral Excursion-6"},
    {32821, "Digastric Open _ Close-1"},
    {32822, "Temporalis Trigger Point-31"},
    {32823, "Masseter Trigger Point-8"},
    {32824, "Scm Trigger Point-14"},
    {32825, "Trapezius Trigger Point-32"},
    {32826, "Medial Pterygoid Trigger Point-9"},
    {32827, "Lateral Pterygoid Trigger Point-7"},
    {32828, "Unpacking And Setting Up The K7-33"},
    {32829, "Setup Protocol For Jaw Tracking-30"},
    {32830, "Setup Protocol For Emg-28"},
    {32831, "Setup Protocol For Esg (Joint Sounds)-29"},
    {32832, "J5 Introduction-3"},
    {32833, "J5 Patient Preparation-4"},
    {32835, "Nm Treatment-11"},
    {32836, "Nm Evaluation-10"},
    {32837, "Nm And Tmd-12"},
    {32839, "Duotrode Application-2"},
    {32840, "Open-and-Close-with-muscles"},
    {33331, "Scan 1-15"},
    {33332, "Scan 2-22"},
    {33333, "Scan 3-23"},
    {33334, "Scan 5-25"},
    {33335, "Scan 5-25"},
    {33336, "Scan 6-26"},
    {33338, "Scan 8-27"},
    {33339, "Scan 9-36"},
    {33340, "Scan 10-35"},
    {33840, "Airway-and-TMJ-English"}
};

#endif // VIDEO_DEFAULTS_H