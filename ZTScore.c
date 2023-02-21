#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <time.h>
#include <signal.h>
#include <omp.h>
#include <sys/types.h>
#include <unistd.h>

#define  LOG_TAG    "zgf"

// #include <android/log.h>
// #define  LOGD(...)  {__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__);}
#define LOGD(...) {printf(LOG_TAG);printf(" pid:%d openmpid:%d ", getpid(), omp_get_thread_num());printf(__VA_ARGS__);printf("\n");}

#define MAX_WORDS_ONE_LINE 20 // 最大的字数（其实是 19 个字，前面有两个是记录开始时间），建议改到 100
#define CHANNELS  2 // 声道
#define SAMPLE_RATE  48000 // 采样率
#define MAX_DISTANCE 2147483647
#define BITS 16 // 采样位深
#define MIN(X, Y) ((X)<(Y)?(X) : (Y))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define LIMIT(a) (MAX(0, MIN(1, a)))

char* getsubstr(char *s, regmatch_t *pmatch)
{
    static char buf[100] = {0};
    memset(buf, 0, sizeof(buf));
    memcpy(buf, s+pmatch->rm_so, pmatch->rm_eo - pmatch->rm_so);
    return buf;
}


struct IndexRange {
    int length;
    int * data;
};

float get_rms(int pcmList[], int length){
    double sum = 0;
    int ma = 0;
    int mi = 9999999;
    for (int index=0; index<length; index++){
        if (ma < pcmList[index]){ma = pcmList[index];}
        if (mi > pcmList[index]){mi = pcmList[index];}
    }
    // LOGD("get_rms status, pcmList, max:%d, min:%d, length:%d", ma, mi, length);
    for (int index = 0; index < length; index ++){
        // if (index % 10000 == 0){LOGD("int get_rms, sum:%f, value:%d", sum, pcmList[index]);}
        sum += (double) pcmList[index];
    }
    // LOGD("in get_rms, sum done");
    float res = (float) sqrt(sum / (length + 1));
    return res;
}


float get_dBFS(int pcmList[], int length, int bits){
    float rms = get_rms(pcmList, length);
    return (float) 20 * log10(rms/pow(2, bits - 1) + 0.00001);
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
        costMatrix[index][0] = (float) MAX_DISTANCE;
    }
    for (int index =1; index<userLength + 1; index++){
        costMatrix[0][index] = (float) MAX_DISTANCE;
    }
    costMatrix[0][0] = 0;

    for (int indexX = 1; indexX < singerLength + 1; indexX++){
        for (int indexY = 1; indexY < userLength + 1; indexY ++){
            float x = costMatrix[indexX - 1][indexY];
            float y = costMatrix[indexX - 1][indexY - 1];
            float z = costMatrix[indexX][indexY - 1];
            float minValue = MIN(MIN(x, y), z);
            float distance = distanceMatric[indexX - 1][indexY - 1];
            //LOGD("min distance:%d distance:%d, indexx:%d, indexxy:%d, x:%d, y:%d, z:%d, i,2:%d\n",
            //  minValue, distance, indexX, indexY, x, y, z, costMatrix[1][2]);
            costMatrix[indexX][indexY] = minValue + distance;
            //LOGD("update x:%d, y:%d, value:%d\n", indexX, indexY, costMatrix[indexX][indexY]);
        }
    }
    return costMatrix[singerLength][userLength];
}

struct IndexRange get_notSilenceRangeLogic(int lyricIndex, int audio[], int length, int silence_min, float dbfs, int channels, int sr, int bits){
    // struct IndexRange defaultNotSilenceRangeList = {0,  (int *)malloc( 2 * (0 + 1) * sizeof(int))};
    // return defaultNotSilenceRangeList;

    LOGD("get not silence range, default value, lyricIndex:%d, audioLength:%d, silenceMin:%d, dbfs:%.2f, channels:%d, sr:%d, bits:%d",
         lyricIndex, length, silence_min, dbfs, channels, sr, bits
    );

    // 非静默区间

    float reversThreshold = reverse_dBFS(dbfs, bits);
    int seek_step = 10.0 / 1000 * channels * sr;
    int minSilenceFrams = (int) (1.0 * silence_min / 1000 * channels * sr);

    int caculateCount = (length - minSilenceFrams) / seek_step;
    int *caculateStartIndex = (int *)malloc(sizeof(int) * caculateCount);
    memset(caculateStartIndex, 0, sizeof(int) * caculateCount);

    for (int index=0; index < caculateCount; index ++){
        caculateStartIndex[index] = index * seek_step;
    }

    int *silenceFlagList = (int *)malloc(caculateCount * sizeof(int));
    for (int i=0;i<caculateCount;i++){silenceFlagList[i]=0;}


    int partPcmArrayForRms[48000 * 2 * 1000/1000] = {0};
    
    LOGD("ready for loop done:%d", audio[length-1]);

    for (int index =0; index < caculateCount; index ++){
        float rms = 0.;
        int startIndex = caculateStartIndex[index];

        // int* partPcmArrayForRms = (int *)malloc(1000 * sizeof(int));
        int* partPcmArrayForRms = (int *)malloc(minSilenceFrams * sizeof(int));
        // LOGD("minSilenceFrams:%d", minSilenceFrams);
        memset(partPcmArrayForRms, 0, minSilenceFrams * sizeof(int));

        int cutAudioLength = 0;
        for (int pcmIndex=0; (pcmIndex < minSilenceFrams) & ((startIndex + pcmIndex) < length); pcmIndex ++) {
            partPcmArrayForRms[pcmIndex] = audio[startIndex + pcmIndex];
            cutAudioLength ++;
        }

        rms = get_rms(partPcmArrayForRms, cutAudioLength);
        free(partPcmArrayForRms);

        if (rms > reversThreshold){
            silenceFlagList[index] = 1;
        }
//        LOGD("lyric:%d, startIdx:%d, minSilenceFrams:%d, maxFrame:%d, endIdx:%d, rms:%.2f, flag:%d, reversThreshold:%.2f",
//            lyricIndex, startIndex, minSilenceFrams, length, startIndex + minSilenceFrams,
//            rms, silenceFlagList[index], reversThreshold);
    }
    LOGD(">>> get rms loop done");

    
    // 取静默区间
    int silenceCount = 0;
    for (int index=0; index < caculateCount; index++){
        if (silenceFlagList[index] == 0){
            silenceCount ++;
        }
    }

    int silenceIndexList[silenceCount];
    memset(silenceIndexList, 0 , silenceCount * sizeof(int));

    int updateSilenceIndex = 0;
    for (int index = 0; index < caculateCount; index ++){
        if (silenceFlagList[index] == 0){
            silenceIndexList[updateSilenceIndex] = caculateStartIndex[index];
            updateSilenceIndex ++;
        }
    }
    free(silenceFlagList);
    free(caculateStartIndex);

//    LOGD("silence range count: %d", updateSilenceIndex);
//    for (int __index=0; __index < updateSilenceIndex/2; __index++){
//        LOGD("silence range: %d -> %d", silenceIndexList[2 * __index], silenceIndexList[2 * __index + 1]);
//    }


    // 提取连续区间
    int silenceIndexRange[(MAX_WORDS_ONE_LINE + 1) * 2];
    memset(silenceIndexRange, 0 , ((MAX_WORDS_ONE_LINE + 1) * 2) * sizeof(silenceIndexRange[0]));

    int prevPointer = silenceIndexList[0];
    int currentPointer = silenceIndexList[0];

    int silenceRangeIndex = 1;
    for (int index =1; index < silenceCount; index ++){
//        LOGD("index:%d, prevPointer:%d + seek_step:%d != index value:%d &  index value:%d > prevPointer:%d + minSilenceFrams:%d",
//            index, prevPointer, seek_step, silenceIndexList[index], silenceIndexList[index], prevPointer, minSilenceFrams);
        if (((prevPointer + seek_step) != silenceIndexList[index]) & (silenceIndexList[index] > (prevPointer + minSilenceFrams))){
            silenceIndexRange[2 * silenceRangeIndex] = currentPointer;
            silenceIndexRange[2 * silenceRangeIndex + 1] = prevPointer + minSilenceFrams;
            silenceRangeIndex ++;
//            LOGD("in get_notSilenceRangeLogic, add silence range, ms, %.1f:%.1f, frame, %d:%d",
//                1000.0 * currentPointer/channels/sr, 1000.0 * (prevPointer + minSilenceFrams)/channels/sr,
//                currentPointer, prevPointer + minSilenceFrams);
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
    struct IndexRange notSilenceRangeList = {0,  (int *)malloc( 2 * (silenceRangeIndex + 1) * sizeof(int))};
    memset(notSilenceRangeList.data, 0 , 2 * (silenceRangeIndex + 1) * sizeof(int));

    int notSilenceCount = 0;
    int minDelteSilenceRangeFrames = 0.02 * channels * sr;
    LOGD("channels:%d, sr:%d, min silence frames:%d", channels, sr, minDelteSilenceRangeFrames);

    for (int index=0; index < silenceRangeIndex - 1; index++){
        int notSilenceRangeStartIndex = silenceIndexRange[2*index + 1];
        int notSilenceRangeEndIndex = silenceIndexRange[2*(index + 1)];
        LOGD("in get_notSilenceRangeLogic, get not silence range, index:%d, %d - %d = %d >? %d,  %.1f:%.1f, range index:%d",
             index,
             notSilenceRangeStartIndex,
             notSilenceRangeEndIndex,
             notSilenceRangeEndIndex - notSilenceRangeStartIndex,
             minDelteSilenceRangeFrames,
             1000.0 * notSilenceRangeStartIndex/channels/sr,
             1000.0 * notSilenceRangeEndIndex/channels/sr,
             notSilenceCount);
        if ((notSilenceRangeEndIndex - notSilenceRangeStartIndex) > minDelteSilenceRangeFrames){
            notSilenceRangeList.data[2*(notSilenceCount)] = notSilenceRangeStartIndex;
            notSilenceRangeList.data[2*(notSilenceCount) + 1] = notSilenceRangeEndIndex;
            notSilenceCount ++;
            LOGD("get not silence range, %d:%d",
                 notSilenceRangeList.data[2*(notSilenceCount -1)], notSilenceRangeList.data[2*(notSilenceCount -1) + 1]);
        }
    }
    notSilenceRangeList.length = 2*notSilenceCount;
    return notSilenceRangeList;
}


float* get_LyricMiddleTimes(int lyricTimes[], int lyricTimeLength){
    float* lyricMiddleTimeArray = (float *)malloc(lyricTimeLength * sizeof(float));
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
    float* toTimes = (float *)malloc(rangeCount * sizeof(float));
    for (int index =0; index < rangeCount; index++){
        toTimes[index] = notSilenceRange.data[index] * 1.0/ channels / sr * 1000;
        LOGD("frame to index, frame:%d, ms:%.1f", notSilenceRange.data[index], toTimes[index]);
    }
    return toTimes;
}

struct IndexRange  get_splitMiddleTimes(float addZerorRangeTimes[], int rangeCount){

    float middleTimes[rangeCount];
    int activateRangeCount = 0;
    for (int index =0; index < rangeCount/2; index++){
        if (addZerorRangeTimes[2*index] == 0 & addZerorRangeTimes[2*index + 1] == 0){continue;}
        float value = (float) (addZerorRangeTimes[2*index] + addZerorRangeTimes[2*index + 1]) / 2;
        if (value == 0){continue;}
        middleTimes[activateRangeCount] =  value;
        LOGD("range to middle index, v1:%.1f, v2:%.1f, add value:%.1f",
             addZerorRangeTimes[2*index],
             addZerorRangeTimes[2*index + 1],
             middleTimes[activateRangeCount]);
        activateRangeCount ++;
    }

    struct IndexRange returnStruct = {activateRangeCount, (int *)malloc(activateRangeCount * sizeof(int))};
    for (int index=0; index < activateRangeCount; index++){
        returnStruct.data[index] = (int) middleTimes[index];
    }

    return returnStruct;
}

struct IndexRange get_notSilenceRange(int lyricIndex, int audioFrames[], int framesCount, float lyricMiddleTimeArray[], int lyricRangeCount, int channels, int sr, int bits){
    struct IndexRange returnNotSilenceRange = {0, (int *)malloc(2 * sizeof(int))};
    // normal 20230218
    // return returnNotSilenceRange; // crash /// bash compile_c.sh  549594.66s user 8942.35s system 517% cpu 29:58:58.26 total // *** Incorrect guard value: 953482739915
    float dBFS = get_dBFS(audioFrames, framesCount, bits);
    // LOGD("source frams:%d, bits:%d, fs:%.3f", framesCount, bits, dBFS);
    // return returnNotSilenceRange; //crash // bash compile_c.sh  67622.47s user 695.85s system 563% cpu 3:21:59.96 total // Incorrect guard value: 1760936591600

    int min_silence_ms[4] = {110, 90, 70, 50};
    float dBFS_rate[8] = {0.9, 0.95, 1, 1.05, 1.1, 1.15, 1.2, 1.25};

    float minScore = 999999;

    // return returnNotSilenceRange;
    int loopCount = 0;
    for (int silence_ms_index = 0; silence_ms_index < sizeof(min_silence_ms)/sizeof(min_silence_ms[0]); silence_ms_index ++ ){
        for (int dBFS_index = 0; dBFS_index < sizeof(dBFS_rate)/sizeof(dBFS_rate[0]); dBFS_index ++) {

            // LOGD("loopCount:%d  silence_ms:%d, dBFS:%f",
            //     loopCount, min_silence_ms[silence_ms_index], dBFS_rate[dBFS_index]);

            loopCount ++;

            // 单位 frame, [length, (frame1, frame2, ...)]
            struct IndexRange  notSilenceRange = get_notSilenceRangeLogic(
                    lyricIndex,
                    audioFrames,
                    framesCount,
                    min_silence_ms[silence_ms_index],
                    dBFS * dBFS_rate[dBFS_index],
                    channels,
                    sr,
                    bits);

            // 单位 ms
            // LOGD("not silence range length:%d", notSilenceRange.length);
            float* splitRangeTimes = get_frameIndexToTimeIndex(notSilenceRange, channels, sr);
            struct IndexRange middleTimeArrayStruct = get_splitMiddleTimes(splitRangeTimes, notSilenceRange.length);
            float middleTimeArray[middleTimeArrayStruct.length];

            for (int __index=0; __index < middleTimeArrayStruct.length; __index++){
                middleTimeArray[__index] = middleTimeArrayStruct.data[__index];
            }
            int middleTimeArrayLength = middleTimeArrayStruct.length;

            // 参数评价指标
            float minDistance = dtw(lyricMiddleTimeArray, lyricRangeCount, middleTimeArray, middleTimeArrayLength);
            float rangeScore = 10000 * abs(notSilenceRange.length/2 - lyricRangeCount)  + minDistance;

            // LOGD("lyric count:%d, split count:%d, min distance:%.3f, mean:%.3f, rangeScore:%.3f, minScore:%.3f",
            //     lyricRangeCount, middleTimeArrayLength, minDistance, minDistance/lyricRangeCount, rangeScore, minScore);

            if ( rangeScore < minScore){
                returnNotSilenceRange = notSilenceRange;
                for (int _index=0; _index < returnNotSilenceRange.length/2; _index++){
                    // LOGD("better split, start:%d, end:%d",
                    //     returnNotSilenceRange.data[2*_index], returnNotSilenceRange.data[2 * _index + 1]);
                }
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
    for (int index = 0; index < splitTimesLength;  index ++){
        //LOGD("split time, index:%d, %.1f", index, splitTimes[index]);
    }
    for (int index = 0; index < lyricTimesLength;  index ++){
        //LOGD("lyric time, index:%d, %.1f", index, lyricTimes[index]);
    }
    float minDistance = dtw(lyricTimes, lyricTimesLength, splitTimes, splitTimesLength);
    float meanDistance = minDistance / lyricTimesLength;
    float grade =  1 - MAX(0, (meanDistance - 200)/1000) - 0.1 * abs(lyricTimesLength - splitTimesLength);
    // LOGD("[grade]>>>>>>> min distance:%.3f, mean distance:%.3f, grade:%.5f", minDistance, meanDistance, grade);
    return grade;
}

float get_emotionGrade(float splitTimes[], int splitRangeCount, float dbfsArrays[], int dbfsLength){

    float timeMin = 99999999.0;
    float timeMax = 0;
    for (int index=0;index<splitRangeCount;index++){
        float value  = fabs(splitTimes[2 * index] - splitTimes[2 * index + 1]);
        if (value > timeMax){timeMax = value;}
        if (value < timeMin){timeMin = value;}
    }
    float durationsGrade = 1 - timeMin / timeMax;
    // LOGD("声音持续, 最大:%.3f, 最小:%.3f, 得分:%.3f", timeMax, timeMin, durationsGrade);


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
    float dbfsGrade = fabs(dbfsMax - dbfsMin)/15;

    // 增加异常判断
    if (dbfsMax < -80){dbfsGrade=0;}
    if (dbfsMin > -30){dbfsGrade=0;}
    if (fabs(dbfsMax - dbfsMin) > 12){dbfsGrade=0;}

    // LOGD("音频幅度, 最大:%.3f, 最小:%.3f, 得分:%.3f", dbfsMax, dbfsMin, dbfsGrade);

    float returnGrade = 0.7 * durationsGrade + 0.3 * dbfsGrade;
    // LOGD("发声时长情感分:%.3f, 发声音量情感分:%.3f, 加权:%.3f", durationsGrade, dbfsGrade, returnGrade);
    return returnGrade;
}

float *innerCalculateGrade(int cutAudioFrame[], int segmentFrames, int wordLineTime[], int existsTimeRangeCount) {
    static float res[2];
    res[0] = 0;
    res[1] = 1;

    if (segmentFrames < 48000 * 2 * 0.1){
        LOGD("ERROR, audio length error: < 0.1S");
        return res;
    }
    // return res; // 20230215 run 18 h normal
    // 取得歌词每个字的 (t1, t2) 中间值
    float* lyricMiddleTimes = get_LyricMiddleTimes(wordLineTime, existsTimeRangeCount);


    // 非静音区间
    // lineIndex 打印用
    // cutAudioFrame 为 pcm
    // segmentFrames 为 pcm 长度
    // 歌词中间值数组
    // existsTimeRangeCount 歌词有多少个字 - 1，后面的 3 个可以外边传进来）
    struct IndexRange notSilenceRange = get_notSilenceRange(
            0,
            cutAudioFrame,
            segmentFrames,
            lyricMiddleTimes,
            existsTimeRangeCount,
            CHANNELS,
            SAMPLE_RATE,
            BITS
    );

    int rangeNum = notSilenceRange.length/2;
    // LOGD("============== bestest result:%d length:%d ================", 0, rangeNum);
    for (int index =0; index < rangeNum; index ++){
        LOGD("best range [%.0f, %.0f], [%d:%d]",
             1.0 * notSilenceRange.data[2*index] / CHANNELS / SAMPLE_RATE * 1000,
             1.0 * notSilenceRange.data[2*index + 1] / CHANNELS / SAMPLE_RATE * 1000,
             notSilenceRange.data[2*index],
             notSilenceRange.data[2*index + 1]
        );
    }

    // 节奏分
    float* splitRangeTimes = get_frameIndexToTimeIndex(notSilenceRange, CHANNELS, SAMPLE_RATE);
    struct IndexRange splitMiddleTimesStruct = get_splitMiddleTimes(splitRangeTimes, notSilenceRange.length);
    float splitMiddleTimes[splitMiddleTimesStruct.length];
    for (int index =0; index < splitMiddleTimesStruct.length; index++){
        splitMiddleTimes[index] = splitMiddleTimesStruct.data[index];
    }
    float rhythmGrade = get_rhythmGrade(
            lyricMiddleTimes, existsTimeRangeCount - 1, splitMiddleTimes, splitMiddleTimesStruct.length);

    rhythmGrade = LIMIT(rhythmGrade);
    // LOGD("节奏分:%.3f", rhythmGrade);
    free(notSilenceRange.data);notSilenceRange.data=NULL;
    free(lyricMiddleTimes);
    free(splitRangeTimes);
    free(splitMiddleTimesStruct.data);splitMiddleTimesStruct.data=NULL;
    return res;

    // 情感分
    float splitDbfs[notSilenceRange.length/2];
    for (int index=0;index<notSilenceRange.length/2;index++){
        int startFrameIndex = notSilenceRange.data[2*index];
        int endFrameIndex = notSilenceRange.data[2*index + 1];
        int copyLength = endFrameIndex - startFrameIndex + 1;
        int *partPcmArray = (int *)malloc(copyLength * sizeof(int));
        // int partPcmArray[1000]= {0};
        int updateLength = 0;
        for (int copyFrameIndex=0; (copyFrameIndex < copyLength) & (segmentFrames > startFrameIndex + copyFrameIndex); copyFrameIndex++){
            int value = cutAudioFrame[startFrameIndex + copyFrameIndex];
            partPcmArray[copyFrameIndex] = value;
            updateLength ++;
        }
        splitDbfs[index] = get_dBFS(partPcmArray, updateLength, BITS);
        free(partPcmArray);
    }
    float emotionGrade = get_emotionGrade(splitRangeTimes, splitMiddleTimesStruct.length, splitDbfs, notSilenceRange.length/2);
    emotionGrade = LIMIT(emotionGrade);
    // LOGD("情感分:%.3f", emotionGrade);

    free(cutAudioFrame);cutAudioFrame=NULL;
    free(wordLineTime);wordLineTime=NULL;
    free(notSilenceRange.data);notSilenceRange.data=NULL;
    free(splitRangeTimes);
    free(splitMiddleTimesStruct.data);splitMiddleTimesStruct.data=NULL;
    free(lyricMiddleTimes);lyricMiddleTimes=NULL;
    // return res;

    // free(splitRangeTimes);
    // free(lyricMiddleTimes);
    // free(splitMiddleTimesStruct.data);splitMiddleTimesStruct.data=NULL;
    // free(notSilenceRange.data);notSilenceRange.data=NULL;
    // free(cutAudioFrame);cutAudioFrame=NULL;
    // free(wordLineTime);wordLineTime=NULL;

    res[0] = rhythmGrade;
    res[1] = emotionGrade;

    LOGD("ALL DONE ~~~")

    return res;
}

void sig_func(int sig)
{
    LOGD("error with signal:%d", sig);
}

float *calculateGrade(unsigned char pcmList[], int pcmLength, int lyricList[], int lyricLength) {
    /*奔溃信号处理*/
    signal(SIGFPE, sig_func);
    signal(SIGSEGV, sig_func);

    /*正式逻辑*/
    float *ret = (float *)malloc(2 * sizeof(float));
    ret[0] = 0;ret[1]=0;
    float *lRes = NULL;

    int *intPcmList;
    int intPcmLength = pcmLength / 2;
    intPcmList = (int *)malloc(sizeof(int) * intPcmLength);
    memset(intPcmList, 0, sizeof(int) * intPcmLength);
    for (int i = 0; i < intPcmLength; i++){
        int value = ((int)((pcmList[2*i + 1]<<8) | pcmList[2*i]));

        int transValue = value;
        if (transValue > 32767){transValue = value - 65536;}

        // int newValue = (int) (pow(1.0 * transValue / 128, 2)); error
        int newValue = (int)(1.0 * transValue / 32768 * 256 * transValue / 32768 * 256);

        if (i < 960){
            LOGD("raw value:%d,%d > %d, transf value:%d, to 8bit^2:%d", pcmList[2*i], pcmList[2*i + 1], value, transValue, newValue);
        }

        intPcmList[i] = newValue;
    }

    lRes = innerCalculateGrade(intPcmList, intPcmLength, lyricList, lyricLength);
    LOGD("caculate return, %.3f,%.3f", lRes[0], lRes[1]);

    if (NULL != lRes){
        LOGD("update return array");
        ret[0] = lRes[0];
        ret[1] = lRes[1];
    }
    // free(intPcmList);intPcmList=NULL;
    // free(lyricList);lyricList=NULL;
    free(lRes);
    LOGD("ready return, %.3f,%.3f", ret[0], ret[1]);
    return ret;
}


// ANDROID
float *calculateGradeInt(int pcmList[], int pcmLength, int lyricList[], int lyricLength) {
    return innerCalculateGrade(pcmList, pcmLength, lyricList, lyricLength);
}

float *testCharData(unsigned char pcmList[], int pcmLength, int lyricList[], int lyricLength) {
    float* resultData = (float*)malloc(2);
    resultData[0] = pcmList[0];
    resultData[1] = pcmLength + lyricLength;
    return resultData;
}

float *testStringData(unsigned char pcmList[], int pcmLength, int lyricList[], int lyricLength) {
    float* resultData = (float*)malloc(2);
    resultData[0] = pcmList[0];
    resultData[1] = pcmLength + lyricLength;
    return resultData;
}


// TEST
int randomValueTestWholeLogic(){
    int pcmMsTimeLength = 5000 + rand() % 5000;

    // generate pcm
    int length = (int) (48000 * pcmMsTimeLength * 2 / 1000);

    int* samplePcmArray = (int*) malloc(sizeof(int) * length);
    for (int index=0; index < length; index++){
        int sampleValue = rand() % 65536;
        samplePcmArray[index] = sampleValue;
    }

    // generate lyric
    int* lyricArray = (int*) malloc(sizeof(int) * 30 * 2);
    int lyricCount = 0;
    int timeStart = 0;
    while (1==1){
        int startDelta = rand() % 300;
        int singDelta = rand() % 300;
        int timeRangeStart = timeStart + startDelta;
        int timeRangeEnd = timeRangeStart + singDelta;
        // break if time reach max
        if (timeRangeStart > pcmMsTimeLength | timeRangeEnd > pcmMsTimeLength | lyricCount > 30){
            break;
        }

        lyricArray[2 * (lyricCount)] = timeRangeStart;
        lyricArray[2 * (lyricCount) + 1] = timeRangeEnd;
        lyricCount++;
    }

    lyricArray[0] = lyricCount;
    float* res = calculateGradeInt(samplePcmArray, length, lyricArray, lyricCount);
    LOGD("one times done");
    free(samplePcmArray);
    // free(lyricArray);
    return 1;
}

int randomValueTestGetRMS(){
    int msTimeLength = rand() % (1000 * 60 * 5); // 10S = 10 * 1000ms
    int frameLength = msTimeLength * SAMPLE_RATE * CHANNELS / 1000;
    int* pcmArray = (int*) malloc(sizeof(int) * frameLength);
    for (int index=0; index<frameLength; index++){
        pcmArray[index] = pow(rand() % 256, 2);
    }
    get_dBFS(pcmArray, frameLength, BITS);
    free(pcmArray);
    return 1;
}

int main(){
    for (int index=1; index < 100000; index++){
        int loopTimes = 1000;
        omp_set_dynamic(0);     // Explicitly disable dynamic teams
        omp_set_num_threads(6); // Use 4 threads for all consecutive parallel regions
        # pragma omp parallel for
        for (int _index=0; _index < loopTimes; _index++){
            // randomValueTestGetRMS();
            randomValueTestWholeLogic();
        }
        printf("index:%d * %d done\n", index, loopTimes);
    }
    printf("all loop done \n");
    return 1;
}
