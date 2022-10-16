# Project 1 - Acoustic link

## Group members:
```
胡锦添 2020533167
陈溯汀 2020533185
```

## Questions implemented
```
Part1 - Understanding Your Tools
Part2 - Generate Correct Sound
Part3 - Transmitting your first bit
Part4 - Error Correction
Part6 - Need for Speed
```

## User's Guide
### Step 1
Download the library from [JUCE](https://juce.com/get-juce/download), unzip and parse the whole library into project1/JUCE/*

### Step 2
Please edit `CMakeLists.txt` according to the part you are trying:\
In `CMakeLists.txt:3`, we have `set(CURRENT_TARGET "whatever")`\
Set `CURRENT_TARGET` as follows:

| Part to test | CURRENT_TARGET   |
|--------------|------------------|
| Part1        | Project1_Part1_2 |
| Part2        | Project1_Part1_2 |
| Part3        | Project1_Part3   |
| Part4        | Project1_Part4   |
| Part6        | Project1_Part3   |

### Step 3
We used some absolute paths, which mat cause some issues.
You can modify it here: `Part3.h:283`


Contact those emails if there are still any issues:
```
hujt@shanghaitech.edu.cn
chenst@shanghaitech.edu.cn
```


## Miscellaneous
Sadly, we started too late on these bonuses. So we didn't have enough time for Part5.