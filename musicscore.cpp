#include <jni.h>
#include <string>
#include "ZTScore.c"

extern "C" JNIEXPORT jstring JNICALL
Java_com_suishouwan_neon_music_musicscore_MusicScoreLib_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_com_suishouwan_neon_music_musicscore_MusicScoreLib_calculateGradeChar(JNIEnv *env,
                                                                           jobject thiz,
                                                                           jcharArray pcm_list,
                                                                           jint pcm_length,
                                                                           jintArray lyric_list,
                                                                           jint lyric_length) {
    jchar* pcmArr =  env->GetCharArrayElements(pcm_list, nullptr);
    jint* lyricArr = env->GetIntArrayElements(lyric_list, nullptr);

    jfloat* result = calculateGrade(reinterpret_cast<unsigned char *>(pcmArr), pcm_length, lyricArr, lyric_length);

    jfloatArray floatArray = env->NewFloatArray(2);
    env->SetFloatArrayRegion(floatArray, 0, 2, result);
    return floatArray;
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_com_suishouwan_neon_music_musicscore_MusicScoreLib_testCharData(JNIEnv *env, jobject thiz,
                                                                     jcharArray pcm_list,
                                                                     jint pcm_length,
                                                                     jintArray lyric_list,
                                                                     jint lyric_length) {
    jchar* pcmArr =  env->GetCharArrayElements(pcm_list, nullptr);
    jint* lyricArr = env->GetIntArrayElements(lyric_list, nullptr);

    jfloat* result = testCharData(reinterpret_cast<unsigned char *>(pcmArr), pcm_length, lyricArr, lyric_length);

    jfloatArray floatArray = env->NewFloatArray(2);
    env->SetFloatArrayRegion(floatArray, 0, 2, result);
    return floatArray;
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_com_suishouwan_neon_music_musicscore_MusicScoreLib_calculateGradeInt(JNIEnv *env, jobject thiz,
                                                                          jintArray pcm_list,
                                                                          jint pcm_length,
                                                                          jintArray lyric_list,
                                                                          jint lyric_length) {
    jint* pcmArr = env->GetIntArrayElements(pcm_list, nullptr);
    jint* lyricArr = env->GetIntArrayElements(lyric_list, nullptr);

    jfloat* result = calculateGradeInt(pcmArr, pcm_length, lyricArr, lyric_length);

    jfloatArray floatArray = env->NewFloatArray(2);
    env->SetFloatArrayRegion(floatArray, 0, 2, result);
    return floatArray;
}