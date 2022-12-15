#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define  LOG_TAG    "MusicAnalysis_MusicIntonationMgr_qw"

//#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGD(...) {printf(LOG_TAG);printf(" ");printf(__VA_ARGS__);printf("\n");}

#define W  128
#define MAX_WORDS_ONE_LINE 20 // 最大的字数（其实是 19 个字，前面有两个是记录开始时间），建议改到 100
#define CHANNELS  2 // 声道
#define SAMPLE_RATE  48000 // 采样率
#define MAX_DISTANCE 2147483647
#define BITS 16 // 采样位深
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

char* getsubstr(char *s, regmatch_t *pmatch)
{
    static char buf[100] = {0};
    memset(buf, 0, sizeof(buf));
    memcpy(buf, s+pmatch->rm_so, pmatch->rm_eo - pmatch->rm_so);
    return buf;
}


struct IndexRange {
    int length; // 如果是有 n 个字，那么数组长度就是 (n + 1) * 2
    int * data; // [改行歌词的开始时间, -1, t1, t2, t3, t4...]
};

float get_rms(int pcmList[], int length){
    int sum = 0;
    for (int index = 0; index < length; index ++){
        sum += pcmList[index];
    }
    float res = (float) sqrt(sum / length);
    return res;
}


float get_dBFS(int pcmList[], int length, int bits){
    float rms = get_rms(pcmList, length);
    return (float) 20 * log10(rms/pow(2, bits - 1));
}


float reverse_dBFS(float dbfs, int bits){
    return (float) pow(10, dbfs/20) * pow(2, bits - 1);
}


float dtw(float singerPcm[], int singerLength, float userPcm[], int userLength){

    float distanceMatric[singerLength][userLength];
    for (int indexX = 0; indexX < singerLength; indexX++){
        for (int indexY = 0; indexY < userLength; indexY ++){
            distanceMatric[indexX][indexY] = fabs(singerPcm[indexX] - userPcm[indexY]);
        }
    }

    float costMatrix[singerLength + 1][userLength + 1];
    for (int index =1; index<singerLength + 1; index++){
        costMatrix[index][0] = MAX_DISTANCE;
    }
    for (int index =1; index<userLength + 1; index++){
        costMatrix[0][index] = MAX_DISTANCE;
    }
    costMatrix[0][0] = 0;

    for (int indexX = 1; indexX < singerLength + 1; indexX++){
        for (int indexY = 1; indexY < userLength + 1; indexY ++){
            float x = costMatrix[indexX - 1][indexY];
            float y = costMatrix[indexX - 1][indexY - 1];
            float z = costMatrix[indexX][indexY - 1];
            float minValue = MIN(MIN(x, y), z);
            float distance = distanceMatric[indexX - 1][indexY - 1];
            //LOGD("min distance:%d distance:%d, indexx:%d, indexxy:%d, x:%d, y:%d, z:%d, i,2:%d\n", minValue, distance, indexX, indexY, x, y, z, costMatrix[1][2]);
            costMatrix[indexX][indexY] = minValue + distance;
            //LOGD("update x:%d, y:%d, value:%d\n", indexX, indexY, costMatrix[indexX][indexY]);
        }
    }
    return costMatrix[singerLength][userLength];
}

struct IndexRange  get_notSilenceRangeLogic(int lyricIndex, int audio[], int length, int silence_min, float dbfs, int channels, int sr, int bits){
    // 非静默区间
    
    struct IndexRange notSilenceRangeList2 = {0,  malloc( 2 * (1 + 1) * sizeof(int))};
    memset(notSilenceRangeList2.data, 0 , 2 * (1 + 1) * sizeof(notSilenceRangeList2.data[0]));

    float reversThreshold = reverse_dBFS(dbfs, bits);
    int seek_step = 10.0 / 1000 * channels * sr;
    int minSilenceFrams = (int) (1.0 * silence_min / 1000 * channels * sr);

    int caculateCount = (length - minSilenceFrams) / seek_step + 1;
    int *caculateStartIndex = malloc(sizeof(int) * caculateCount);
    memset(caculateStartIndex, 0, sizeof(int) * caculateCount);
    
    for (int index=0; index < caculateCount; index ++){
        caculateStartIndex[index] = index * seek_step;
    }
    
    int *silenceFlagList = malloc(caculateCount * sizeof(int));
    for (int i=0;i<caculateCount;i++){silenceFlagList[i]=0;}

    int *partPcmList = malloc(minSilenceFrams * sizeof(int));

    for (int index =0; index < caculateCount; index ++){
        int startIndex = caculateStartIndex[index];

        memset(partPcmList, 0, minSilenceFrams * sizeof(int));
//        LOGD("lyricIndex:%d, start index:%d, minSilenceFrams:%d, max frame:%d, end index:%d, ", lyricIndex, startIndex, minSilenceFrams, length, startIndex + minSilenceFrams);

        for (int pcmIndex=0; pcmIndex < minSilenceFrams; pcmIndex ++) {
            partPcmList[pcmIndex] = audio[startIndex + pcmIndex];
        }
        float rms = get_rms(partPcmList, minSilenceFrams);
//        LOGD(" get rms: < %.2f>", rms);

        if (rms > reversThreshold){
            silenceFlagList[index] = 1;
        }
//        LOGD(" flag: < %d>, reversThreshold:< %.1f>\n", silenceFlagList[index], reversThreshold);
    }

    // 取静默区间
    int silenceCount = 0;
    for (int index=0; index < caculateCount; index++){
        int silenceFlag = silenceFlagList[index];
        if (silenceFlag == 0){
            silenceCount ++;
        }
    }
    LOGD("silence count:%d", silenceCount);

    int silenceIndexList[silenceCount];
    memset(silenceIndexList, 0 , silenceCount * sizeof(silenceIndexList[0]));

    int updateSilenceIndex = 0;
    for (int index = 0; index < caculateCount; index ++){
        if (silenceFlagList[index] == 0){
            silenceIndexList[updateSilenceIndex] = caculateStartIndex[index];
            updateSilenceIndex ++;
        }
    }

    LOGD("======== silence index ======updateSilenceIndex:%d\n", updateSilenceIndex);
    for (int __index=0; __index < updateSilenceIndex/2; __index++){
        LOGD("silence range %d -> %d", silenceIndexList[2 * __index], silenceIndexList[2 * __index + 1]);
    }


    // 提取连续区间
    int silenceIndexRange[(MAX_WORDS_ONE_LINE + 1) * 2];
    memset(silenceIndexRange, 0 , ((MAX_WORDS_ONE_LINE + 1) * 2) * sizeof(silenceIndexRange[0]));

    int prevPointer = silenceIndexList[0];
    int currentPointer = silenceIndexList[0];

    int silenceRangeIndex = 1;
    for (int index =1; index < silenceCount; index ++){
        /*LOGD("index:%d, prevPointer:%d + seek_step:%d != index value:%d &  index value:%d > prevPointer:%d + minSilenceFrams:%d\n", index, prevPointer, seek_step, silenceIndexList[index], silenceIndexList[index], prevPointer, minSilenceFrams);*/
        if (((prevPointer + seek_step) != silenceIndexList[index]) & (silenceIndexList[index] > (prevPointer + minSilenceFrams))){
            silenceIndexRange[2 * silenceRangeIndex] = currentPointer;
            silenceIndexRange[2 * silenceRangeIndex + 1] = prevPointer + minSilenceFrams;
            silenceRangeIndex ++;
            // LOGD("in get_notSilenceRangeLogic, add silence range, %.1f:%.1f\n", 1000.0 * currentPointer/channels/sr, 1000.0 * (prevPointer + minSilenceFrams)/channels/sr);
            // LOGD(" >>>> in get_notSilenceRangeLogic, add silence range, %d:%d\n", currentPointer, prevPointer + minSilenceFrams);
            currentPointer = silenceIndexList[index];
        }
        if (silenceRangeIndex >= MAX_WORDS_ONE_LINE)break;
        prevPointer = silenceIndexList[index];
    }
    silenceIndexRange[2 * silenceRangeIndex] = currentPointer;
    silenceIndexRange[2 * silenceRangeIndex + 1] = prevPointer + minSilenceFrams;
    silenceRangeIndex ++;
    
    LOGD("silenceRangeIndex:%d", silenceRangeIndex);
    
    for (int index =0; index < silenceRangeIndex; index ++){
        LOGD("range index:%d, start:%d, end:%d", index, silenceIndexRange[2 * index], silenceIndexRange[2 * index + 1]);
    }

    // 非静默区间
    struct IndexRange notSilenceRangeList = {0,  malloc( 2 * (silenceRangeIndex + 1) * sizeof(int))};
    memset(notSilenceRangeList.data, 0 , 2 * (silenceRangeIndex + 1) * sizeof(notSilenceRangeList.data[0]));

    int loopStartIndex = 0;
    // if (silenceIndexRange[1] == 0 & silenceIndexRange[2] == 0){loopStartIndex = 1;}
    int notSilenceCount = 0;
    int minDelteSilenceRangeFrames = 0.02 * channels * sr;
    for (int index=loopStartIndex; index < silenceRangeIndex - 1; index++){
        int notSilenceRangeStartIndex = silenceIndexRange[2*index + 1];
        int notSilenceRangeEndIndex = silenceIndexRange[2*(index + 1)];
        /*LOGD("in get_notSilenceRangeLogic, get not silence range, index:%d, %d - %d = %d >? %d,  %f:%f, range index:%d \n",
                index,
                notSilenceRangeStartIndex,
                notSilenceRangeEndIndex,
                notSilenceRangeEndIndex - notSilenceRangeStartIndex,
                minDelteSilenceRangeFrames,
                1000.0 * notSilenceRangeStartIndex/channels/sr,
                1000.0 * notSilenceRangeEndIndex/channels/sr,
                notSilenceCount);*/
        if ((notSilenceRangeEndIndex - notSilenceRangeStartIndex) > minDelteSilenceRangeFrames){
            notSilenceRangeList.data[2*(notSilenceCount)] = notSilenceRangeStartIndex;
            notSilenceRangeList.data[2*(notSilenceCount) + 1] = notSilenceRangeEndIndex;
            notSilenceCount ++;
            LOGD("get not silence range, %d:%d", notSilenceRangeList.data[2*(index -1)], notSilenceRangeList.data[2*(index -1) + 1]);
        }
    }
    notSilenceRangeList.length = 2*notSilenceCount;
    free(caculateStartIndex);
    free(partPcmList);
    free(silenceFlagList);
    return notSilenceRangeList;
}


float* get_LyricMiddleTimes(int lyricTimes[], int lyricTimeLength){
    float* lyricMiddleTimeArray = malloc(lyricTimeLength * sizeof(float));
    // 取时间中点 单位ms
    for (int index=0; index < lyricTimeLength -1; index++){
        lyricMiddleTimeArray[index] = lyricTimes[2*(index + 1)] + lyricTimes[2*(index + 1) + 1] / 2;
    }
    // 最后一个字超过1S取75%
    if (lyricTimes[2 * (lyricTimeLength - 1) + 1] > 1000){
        lyricMiddleTimeArray[lyricTimeLength - 1] = lyricTimes[2 * (lyricTimeLength - 1)] + lyricTimes[2 * (lyricTimeLength - 1) + 1] * 0.75 / 2;
    }
    return lyricMiddleTimeArray;
}

float* get_frameIndexToTimeIndex(struct IndexRange notSilenceRange, int channels, int sr){
    int rangeCount = notSilenceRange.length;
    float* toTimes = malloc(rangeCount * sizeof(float));
    for (int index =0; index < rangeCount; index++){
        toTimes[index] = notSilenceRange.data[index] * 1000 / channels / sr;
    }
    return toTimes;
}

struct IndexRange  get_splitMiddleTimes(float addZerorRangeTimes[], int rangeCount){

//    LOGD("raw length:%d, times: ", rangeCount);
//    for (int index=0; index < rangeCount; index ++){
//        LOGD("%.1f -> ", addZerorRangeTimes[index]);
//    }
//    LOGD("\n");

    float middleTimes[rangeCount];
    int activateRangeCount = 0;
    for (int index =0; index < rangeCount/2; index++){
        if (addZerorRangeTimes[2*index] == 0 & addZerorRangeTimes[2*index + 1] == 0){continue;}
        float value = (float) (addZerorRangeTimes[2*index] + addZerorRangeTimes[2*index + 1]) / 2;
        if (value == 0){continue;}
        middleTimes[activateRangeCount] =  value;
//        LOGD("v1:%.1f, v2:%.1f, add value:%.1f\n", addZerorRangeTimes[2*index], addZerorRangeTimes[2*index + 1], middleTimes[activateRangeCount]);
        activateRangeCount ++;
    }
    
    struct IndexRange returnStruct = {activateRangeCount, malloc(activateRangeCount * sizeof(int))};
    for (int index=0; index < activateRangeCount; index++){
        returnStruct.data[index] = (int) middleTimes[index];
    }

    /*
    for (int index  =0;index < activateRangeCount; index ++){
        LOGD("for check, %d\n", returnStruct.data[index]);
    }
     */

    return returnStruct;
}

struct IndexRange get_notSilenceRange(int lyricIndex, int audioFrames[], int framesCount, float lyricMiddleTimeArray[], int lyricRangeCount, int channels, int sr, int bits){

    float dBFS = get_dBFS(audioFrames, framesCount, bits);
    // LOGD("source frams:%d, bits:%d, fs:%.3f\n", framesCount, bits, dBFS);

    int min_silence_ms[4] = {110, 90, 70, 50};
    float dBFS_rate[8] = {0.9, 0.95, 1, 1.05, 1.1, 1.15, 1.2, 1.25};

    float minScore = 999999;
    struct IndexRange returnNotSilenceRange = {2, malloc(2 * sizeof(int))};

    int loopCount = 0;
    for (int silence_ms_index = 0; silence_ms_index < sizeof(min_silence_ms)/sizeof(min_silence_ms[0]); silence_ms_index ++ ){
        for (int dBFS_index = 0; dBFS_index < sizeof(dBFS_rate)/sizeof(dBFS_rate[0]); dBFS_index ++) {
            // LOGD(">>>>>>>> >>>>>>> .>>>>>>>>>> loopCount:%d  silence_ms:%d, dBFS:%f\n", loopCount, min_silence_ms[silence_ms_index], dBFS_rate[dBFS_index]);
            loopCount ++;
            // 单位 frame, [()]


            struct IndexRange  notSilenceRange = get_notSilenceRangeLogic(lyricIndex, audioFrames, framesCount, min_silence_ms[silence_ms_index], dBFS * dBFS_rate[dBFS_index], channels, sr, bits);
            
//            continue;
            
            // 单位 ms
            // LOGD("not silence range length:%d\n", notSilenceRange.length);
            float* splitRangeTimes = get_frameIndexToTimeIndex(notSilenceRange, channels, sr);
            struct IndexRange middleTimeArrayStruct = get_splitMiddleTimes(splitRangeTimes, notSilenceRange.length);
            float middleTimeArray[middleTimeArrayStruct.length];

            for (int __index=0; __index < middleTimeArrayStruct.length; __index++){
                middleTimeArray[__index] = middleTimeArrayStruct.data[__index];
            }
            int middleTimeArrayLength = middleTimeArrayStruct.length;


            /*
            LOGD("lyric ");
            for(int __index=0; __index < lyricRangeCount; __index++){
                LOGD("%.1f-> " , lyricMiddleTimeArray[__index]);
            }
            LOGD("\n");
            LOGD("split ");
            for(int __index=0; __index < middleTimeArrayLength; __index++){
                LOGD("%.1f-> " , middleTimeArray[__index]);
            }
            LOGD("\n");
            // 参数评价指标
            LOGD("lyric count:%d, split count:%d\n", lyricRangeCount, middleTimeArrayLength);
            */
            float minDistance = dtw(lyricMiddleTimeArray, lyricRangeCount, middleTimeArray, middleTimeArrayLength);
            float rangeScore = 10000 * abs(notSilenceRange.length/2 - lyricRangeCount)  + minDistance;
            /*LOGD("******************** mark ********************* view up, min distance:%.3f mean:%.3f rangeScore:%.3f, minScore:%.3f****\n",
                    minDistance, minDistance/lyricRangeCount, rangeScore, minScore);*/

            if ( rangeScore < minScore){

                /*LOGD("******************** filter ********************* view up, min distance:%.3f mean:%.3f rangeScore:%.3f, minScore:%.3f****\n",
                    minDistance, minDistance/lyricRangeCount, rangeScore, minScore);*/

                returnNotSilenceRange = notSilenceRange;
                /*
                for (int _index=0; _index < returnNotSilenceRange.length/2; _index++){
                    LOGD("better split, start:%d, end:%d\n", returnNotSilenceRange.data[2*_index], returnNotSilenceRange.data[2 * _index + 1]);
                }
                LOGD("show done\n");
                 */
                minScore = rangeScore;
            } else {
                free(notSilenceRange.data);
                notSilenceRange.data = NULL;
            }
            free(middleTimeArrayStruct.data);middleTimeArrayStruct.data=NULL;
            free(splitRangeTimes);

        }
    }

    return returnNotSilenceRange;

}

float  get_rhythmGrade(float lyricTimes[], int lyricTimesLength, float splitTimes[], int splitTimesLength){
    /*
    LOGD(">>>>>>> splt; ");
    for (int index = 0; index < splitTimesLength;  index ++){
        LOGD("%.1f -> ", splitTimes[index]);
    }
    LOGD("\n");
    LOGD(">>>>>>> lyric; ");
    for (int index = 0; index < lyricTimesLength;  index ++){
        LOGD("%.1f -> ", lyricTimes[index]);
    }
    LOGD("\n");
    */
    float minDistance = dtw(lyricTimes, lyricTimesLength, splitTimes, splitTimesLength);
    float meanDistance = minDistance / lyricTimesLength;
    float grade =  1 - (meanDistance - 200)/1000 - 0.1 * abs(lyricTimesLength - splitTimesLength);
//    LOGD("[grade]>>>>>>> min distance:%.3f, mean distance:%.3f, grade:%.5f \n", minDistance, meanDistance, grade);

    return grade;
}

float get_emotionGrade(float splitTimes[], int splitRangeCount, float dbfsArrays[], int dbfsLength){

    float timeMin = 99999999;
    float timeMax = 0;
    for (int index=0;index<splitRangeCount;index++){
        float value  = fabs(splitTimes[2 * index] - splitTimes[2 * index + 1]);
        if (value > timeMax){timeMax = value;}
        if (value < timeMin){timeMin = value;}
    }
    LOGD("声音持续, 最大:%.3f, 最小:%.3f\n", timeMax, timeMin);

    float durationsGrade = 1 - timeMin / timeMax;

    float dbfsMin = 0;
    float dbfsMax = -1000;
    for (int index=0;index<dbfsLength;index++){
        float value = dbfsArrays[index];
        if (value < -1000){
            continue;
        }
        if (value > dbfsMax){dbfsMax=value;}
        if (value < dbfsMin){dbfsMin=value;}
    }
    LOGD("音频幅度, 最大:%.3f, 最小:%.3f", dbfsMax, dbfsMin);

    float dbfsGrade = 1 - (dbfsMax - dbfsMin)/15;
    LOGD("发声时长情感分:%.3f, 发声音量情感分:%.3f", durationsGrade, dbfsGrade);
    return 0.4 * durationsGrade + 0.6 * dbfsGrade; // 0.5 * (durationsGrade + dbfsGrade) + 0.5 * fabs(durationsGrade - dbfsGrade);
}

float *innerCalculateGrade(int cutAudioFrame[], int segmentFrames, int wordLineTime[], int existsTimeRangeCount) {
    static float res[2];
    res[0] = 0;
    res[1] = 1;
    if (segmentFrames < 48000 * 2 * 0.1){
        LOGD("ERROR, audio length error: < 0.1S");
        return res;
    }
    // 取得歌词每个字的 (t1, t2) 中间值
    float* lyricMiddleTimes = get_LyricMiddleTimes(wordLineTime, existsTimeRangeCount);
    // 非静音区间（lineIndex 打印用，cutAudioFrame 为 pcm，segmentFrames 为 pcm 长度，歌词中间值数组，existsTimeRangeCount 歌词有多少个字 - 1，后面的 3 个可以外边传进来）
    struct IndexRange notSilenceRange = get_notSilenceRange(0, cutAudioFrame, segmentFrames, lyricMiddleTimes, existsTimeRangeCount, CHANNELS, SAMPLE_RATE, BITS);
    int rangeNum = notSilenceRange.length/2;

    LOGD("============== best result:%d length:%d ================", 0, rangeNum);
    for (int index =0; index < rangeNum; index ++){
        LOGD("best range [%.0f, %.0f]",
            1.0 * notSilenceRange.data[2*index] / CHANNELS / SAMPLE_RATE * 1000,
            1.0 * notSilenceRange.data[2*index + 1] / CHANNELS / SAMPLE_RATE * 1000
        );
    }
    // 节奏分
    float* splitRangeTimes = get_frameIndexToTimeIndex(notSilenceRange, CHANNELS, SAMPLE_RATE);
    struct IndexRange splitMiddleTimesStruct = get_splitMiddleTimes(splitRangeTimes, notSilenceRange.length);
    float splitMiddleTimes[splitMiddleTimesStruct.length];
    for (int index =0; index < splitMiddleTimesStruct.length; index++){
        splitMiddleTimes[index] = splitMiddleTimesStruct.data[index];
    }
    float rhythmGrade = get_rhythmGrade(lyricMiddleTimes, existsTimeRangeCount - 1, splitMiddleTimes, splitMiddleTimesStruct.length);
    if (rhythmGrade > 1) {
        rhythmGrade = 1;
    }
    if (rhythmGrade < 0) {
        rhythmGrade = 0;
    }
    LOGD("节奏分:%.3f\n", rhythmGrade);

    // 情感分
    float splitDbfs[notSilenceRange.length/2];
    LOGD("rms; ");
    LOGD("[segementFramse]->%d ", segmentFrames);
    for (int index=0;index<notSilenceRange.length/2;index++){
        int startFrameIndex = notSilenceRange.data[2*index];
        int endFrameIndex = notSilenceRange.data[2*index + 1];
        int copyLength = endFrameIndex - startFrameIndex + 1;
        int *partPcmArray = malloc(copyLength * sizeof(int));

        for (int copyFrameIndex=0; copyFrameIndex < copyLength; copyFrameIndex++){
            int value = cutAudioFrame[startFrameIndex + copyFrameIndex];
            partPcmArray[copyFrameIndex] = value;
        }
        splitDbfs[index] = get_dBFS(partPcmArray, copyLength, BITS);
        LOGD("%.3f ->", splitDbfs[index]);
        
        free(partPcmArray);
    }
    LOGD("\n");


    float emotionGrade = get_emotionGrade(splitRangeTimes, splitMiddleTimesStruct.length, splitDbfs, notSilenceRange.length/2);
    if (emotionGrade > 1) {
        emotionGrade = 1;
    }
    if (emotionGrade < 0) {
        emotionGrade = 0;
    }
    LOGD("情感分:%.3f\n", emotionGrade);

    free(splitRangeTimes);
    free(lyricMiddleTimes);
    free(splitMiddleTimesStruct.data);splitMiddleTimesStruct.data=NULL;
    free(cutAudioFrame);cutAudioFrame=NULL;
    
    res[0] = rhythmGrade;
    res[1] = emotionGrade;
    return res;
}

float *calculateGrade(unsigned char pcmList[], int pcmLength, int lyricList[], int lyricLength) {
    int *intPcmList;
    int intPcmLength = pcmLength / 2;
    intPcmList = malloc(sizeof(int) * intPcmLength);
    memset(intPcmList, 0, sizeof(int) * intPcmLength);
    for (int i = 0; i < intPcmLength; i++){
        int value = ((int)((pcmList[2*i]<<8) | pcmList[2*i+1])) - 32768;
        int newValue = (int)(value / 32768 * 256 * value / 32768 * 256);
        intPcmList[i] = newValue;
    }
    return innerCalculateGrade(intPcmList, intPcmLength, lyricList, lyricLength);
}

