# ZTScore

## 备注
### 尽量C++风格的C
```
    安卓为cpp
```

### 尽量使用malloc而非数组
```
    ios会有异常情况，原因不明
```

### 切记除法需要小数时*1.0

### 日志统一: define LOGD

### 关于PCM
```
    传输是用unsight char
    储存是小端:array[2*index + 1] << 8 | array[2*index]
    由于使用的是bit, 故>if (transValue > 32767){transValue = value - 65536;}
```
