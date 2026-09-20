//
// Created by HuangLei on 2024/10/7.
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_BASEDEF_H
#define LEARNAV_BASEDEF_H

enum AVFrameFlag {
    kKeyFrame = 1 << 0,  ///<  关键帧
    kFlush = 1 << 1,     ///<  刷新
    kEOS = 1 << 2,       ///<  结束
};

#endif  // LEARNAV_BASEDEF_H
