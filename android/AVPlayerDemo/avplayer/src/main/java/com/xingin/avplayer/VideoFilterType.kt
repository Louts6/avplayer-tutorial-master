package com.xingin.avplayer

/**
 * Video filter types
 */
enum class VideoFilterType(val value: Int) {
    NONE(0),
    FLIP_VERTICAL(1),  ///< 垂直翻转
    GRAY(2),          ///< 灰度
    INVERT(3),        ///< 反色
    STICKER(4);       ///< 贴纸

    companion object {
        fun fromValue(value: Int): VideoFilterType? {
            return values().find { it.value == value }
        }
    }
}
