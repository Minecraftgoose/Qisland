/* Qisland vendored copy — MODIFIED (changes listed in vendor/NOTICE.txt): */
/* @file date.c */
/* INCLUDES */
#include "lunar_date.h"
#include <stdio.h>
#include <string.h>

/* TYPEDEFS */

#define  MIN_YEAR  1900
#define  MAX_YEAR  2099

/* STATIC PROTOTYPES */

/* STATIC VARIABLES */

const static uint32_t  solar_1_1[] = {
       1887,                                                                         0xec04c,  0xec23f, /* 1888-1889 */
    0xec435,   0xec649,  0xec83e,  0xeca51,  0xecc46,  0xece3a,  0xed04d,  0xed242,  0xed436,  0xed64a, /* 1890-1899 */
    0xed83f,   0xeda53,  0xedc48,  0xede3d,  0xee050,  0xee244,  0xee439,  0xee64d,  0xee842,  0xeea36, /* 1900-1909 */
    0xeec4a,   0xeee3e,  0xef052,  0xef246,  0xef43a,  0xef64e,  0xef843,  0xefa37,  0xefc4b,  0xefe41,
    0xf0054,   0xf0248,  0xf043c,  0xf0650,  0xf0845,  0xf0a38,  0xf0c4d,  0xf0e42,  0xf1037,  0xf124a,
    0xf143e,   0xf1651,  0xf1846,  0xf1a3a,  0xf1c4e,  0xf1e44,  0xf2038,  0xf224b,  0xf243f,  0xf2653,
    0xf2848,   0xf2a3b,  0xf2c4f,  0xf2e45,  0xf3039,  0xf324d,  0xf3442,  0xf3636,  0xf384a,  0xf3a3d,
    0xf3c51,   0xf3e46,  0xf403b,  0xf424e,  0xf4443,  0xf4638,  0xf484c,  0xf4a3f,  0xf4c52,  0xf4e48,
    0xf503c,   0xf524f,  0xf5445,  0xf5639,  0xf584d,  0xf5a42,  0xf5c35,  0xf5e49,  0xf603e,  0xf6251,
    0xf6446,   0xf663b,  0xf684f,  0xf6a43,  0xf6c37,  0xf6e4b,  0xf703f,  0xf7252,  0xf7447,  0xf763c,
    0xf7850,   0xf7a45,  0xf7c39,  0xf7e4d,  0xf8042,  0xf8254,  0xf8449,  0xf863d,  0xf8851,  0xf8a46,
    0xf8c3b,   0xf8e4f,  0xf9044,  0xf9237,  0xf944a,  0xf963f,  0xf9853,  0xf9a47,  0xf9c3c,  0xf9e50,
    0xfa045,   0xfa238,  0xfa44c,  0xfa641,  0xfa836,  0xfaa49,  0xfac3d,  0xfae52,  0xfb047,  0xfb23a, /* 2000-2009 */
    0xfb44e,   0xfb643,  0xfb837,  0xfba4a,  0xfbc3f,  0xfbe53,  0xfc048,  0xfc23c,  0xfc450,  0xfc645, /* 2010-2019 */
    0xfc839,   0xfca4c,  0xfcc41,  0xfce36,  0xfd04a,  0xfd23d,  0xfd451,  0xfd646,  0xfd83a,  0xfda4d, /* 2020-2029 */
    0xfdc43,   0xfde37,  0xfe04b,  0xfe23f,  0xfe453,  0xfe648,  0xfe83c,  0xfea4f,  0xfec44,  0xfee38, /* 2030-2039 */
    0xff04c,   0xff241,  0xff436,  0xff64a,  0xff83e,  0xffa51,  0xffc46,  0xffe3a, 0x10004e, 0x100242, /* 2040-2049 */
    0x100437, 0x10064b, 0x100841, 0x100a53, 0x100c48, 0x100e3c, 0x10104f, 0x101244, 0x101438, 0x10164c, /* 2050-2059 */
    0x101842, 0x101a35, 0x101c49, 0x101e3d, 0x102051, 0x102245, 0x10243a, 0x10264e, 0x102843, 0x102a37, /* 2060-2069 */
    0x102c4b, 0x102e3f, 0x103053, 0x103247, 0x10343b, 0x10364f, 0x103845, 0x103a38, 0x103c4c, 0x103e42, /* 2070-2079 */
    0x104036, 0x104249, 0x10443d, 0x104651, 0x104846, 0x104a3a, 0x104c4e, 0x104e43, 0x105038, 0x10524a, /* 2080-2089 */
    0x10543e, 0x105652, 0x105847, 0x105a3b, 0x105c4f, 0x105e45, 0x106039, 0x10624c, 0x106441, 0x106635, /* 2090-2099 */
    0x106849, 0x106a3d, 0x106c51, 0x106e47, 0x10703c, 0x10724f, 0x107444, 0x107638, 0x10784c, 0x107a3f, /* 2100-2109 */
    0x107c53, 0x107e48,                                                                                 /* 2110-2111 */
};

const static uint32_t lunar_month_days[] = {
     1887,                                                          0x1694, 0x16aa,     /* 1888-1889 */
    0x4ad5, 0xab6,  0xc4b7, 0x4ae,  0xa56,  0xb52a, 0x1d2a, 0xd54,  0x75aa, 0x156a,     /* 1890-1899 */
    0x1096d,0x95c,  0x14ae, 0xaa4d, 0x1a4c, 0x1b2a, 0x8d55, 0xad4,  0x135a, 0x495d,     /* 1900-1909 */
    0x95c,  0xd49b, 0x149a, 0x1a4a, 0xbaa5, 0x16a8, 0x1ad4, 0x52da, 0x12b6, 0xe937,     /* 1910-1919 */
    0x92e,  0x1496, 0xb64b, 0xd4a,  0xda8,  0x95b5, 0x56c,  0x12ae, 0x492f, 0x92e,      /* 1920-1929 */
    0xcc96, 0x1a94, 0x1d4a, 0xada9, 0xb5a,  0x56c,  0x726e, 0x125c, 0xf92d, 0x192a,     /* 1930-1939 */
    0x1a94, 0xdb4a, 0x16aa, 0xad4,  0x955b, 0x4ba,  0x125a, 0x592b, 0x152a, 0xf695,     /* 1940-1949 */
    0xd94,  0x16aa, 0xaab5, 0x9b4,  0x14b6, 0x6a57, 0xa56,  0x1152a,0x1d2a, 0xd54,      /* 1950-1959 */
    0xd5aa, 0x156a, 0x96c,  0x94ae, 0x14ae, 0xa4c,  0x7d26, 0x1b2a, 0xeb55, 0xad4,      /* 1960-1969 */
    0x12da, 0xa95d, 0x95a,  0x149a, 0x9a4d, 0x1a4a, 0x11aa5,0x16a8, 0x16d4, 0xd2da,     /* 1970-1979 */
    0x12b6, 0x936,  0x9497, 0x1496, 0x1564b, 0xd4a, 0xda8,  0xd5b4, 0x156c, 0x12ae,     /* 1980-1989 */
    0xa92f, 0x92e,  0xc96,  0x6d4a, 0x1d4a, 0x10d65, 0xb58, 0x156c, 0xb26d, 0x125c,     /* 1990-1999 */
    0x192c, 0x9a95, 0x1a94, 0x1b4a, 0x4b55, 0xad4,  0xf55b, 0x4ba,  0x125a, 0xb92b,     /* 2000-2009 */
    0x152a, 0x1694, 0x96aa, 0x15aa, 0x12ab5, 0x974, 0x14b6, 0xca57, 0xa56,  0x1526,     /* 2010-2019 */
    0x8e95, 0xd54,  0x15aa, 0x49b5,  0x96c, 0xd4ae, 0x149c, 0x1a4c, 0xbd26, 0x1aa6,     /* 2020-2029 */
    0xb54,  0x6d6a, 0x12da, 0x1695d, 0x95a, 0x149a, 0xda4b, 0x1a4a, 0x1aa4, 0xbb54,     /* 2030-2039 */
    0x16b4, 0xada,  0x495b, 0x936,  0xf497, 0x1496, 0x154a, 0xb6a5, 0xda4,  0x15b4,     /* 2040-2049 */
    0x6ab6, 0x126e, 0x1092f, 0x92e, 0xc96, 0xcd4a,  0x1d4a, 0xd64,  0x956c, 0x155c,     /* 2050-2059 */
    0x125c, 0x792e, 0x192c, 0xfa95, 0x1a94, 0x1b4a, 0xab55, 0xad4,  0x14da, 0x8a5d,     /* 2060-2069 */
    0xa5a,  0x1152b, 0x152a, 0x1694, 0xd6aa, 0x15aa, 0xab4, 0x94ba, 0x14b6, 0xa56,      /* 2070-2079 */
    0x7527, 0xd26,  0xee53, 0xd54,  0x15aa, 0xa9b5, 0x96c,  0x14ae, 0x8a4e, 0x1a4c,     /* 2080-2089 */
    0x11d26,0x1aa4, 0x1b54, 0xcd6a, 0xada,   0x95c, 0x949d, 0x149a, 0x1a2a, 0x5b25,     /* 2090-2099 */
    0x1aa4, 0xfb52, 0x16b4, 0xaba,  0xa95b, 0x936,  0x1496, 0x9a4b, 0x154a, 0x136a5,    /* 2100-2109 */
    0xda4,  0x15ac,                                                                     /* 2110-2111 */
};

static const uint8_t DAY_OF_MONTH[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const uint32_t DAY_OF_YEAR[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

// 农历月的名称
const char * MONTH_CN[]       = { "月", "正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊" };
// const char * DAY_TENFOLD[] = { "十", "初" ,"二", "三" };
const char * DAY_TENS_DIGIT[] = { "初" ,"十", "廿", "卅" };
const char * DAY_UNIT_DIGIT[] = { "日", "一", "二", "三", "四", "五", "六", "七", "八", "九" };

const uint32_t SOLAR_TERM_OFFSET[] = { // 各节气与小寒偏移的分钟数
	     0,  21208,  42467,  63836,  85337, 107014, 128867, 150921,
    173149, 195551, 218072, 240693, 263343, 285989, 308563, 331033,
    353350, 375494, 397447, 419210, 440795, 462224, 483532, 504758,
};

// 天干
const char * TIANGAN[]= { "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸", };

// 地支
const char * DIZHI[] = { "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥", };
// 生肖
const char * ANIMALS[] = { "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪" };

typedef struct _FESTIVAL {
    uint8_t month;
    uint8_t day;
    struct {
        uint8_t date_type:  2;      /* !< 0: 公历日期, 1: 农历日期, 2: 星期, 3: 完整星期 */
        uint8_t is_holiday: 1;      /* !< 是否节假日 */
    } flags;
    char * festival;
} FESTIVAL;

const FESTIVAL festivals[] = {
    { .month = 1,  .day = 1,  .flags = { .is_holiday = 1, .date_type = 0 }, "元旦" },
    { .month = 2,  .day = 14, .flags = { .is_holiday = 0, .date_type = 0 }, "情人节" },
    { .month = 3,  .day = 8,  .flags = { .is_holiday = 0, .date_type = 0 }, "妇女节" },
    { .month = 3,  .day = 12, .flags = { .is_holiday = 0, .date_type = 0 }, "植树节" },
    { .month = 5,  .day = 1,  .flags = { .is_holiday = 1, .date_type = 0 }, "劳动节" },
    { .month = 5,  .day = 4,  .flags = { .is_holiday = 0, .date_type = 0 }, "青年节" },
    { .month = 6,  .day = 1,  .flags = { .is_holiday = 0, .date_type = 0 }, "儿童节" }, 
    { .month = 7,  .day = 1,  .flags = { .is_holiday = 0, .date_type = 0 }, "建党节" },
    { .month = 8,  .day = 1,  .flags = { .is_holiday = 0, .date_type = 0 }, "建军节" },
    { .month = 9,  .day = 10, .flags = { .is_holiday = 0, .date_type = 0 }, "教师节" },
    { .month = 10, .day = 1,  .flags = { .is_holiday = 1, .date_type = 0 }, "国庆节" },
    { .month = 12, .day = 24, .flags = { .is_holiday = 0, .date_type = 0 }, "平安夜" },
    { .month = 12, .day = 25, .flags = { .is_holiday = 0, .date_type = 0 }, "圣诞节" },

    { .month = 1,  .day = 1,  .flags = { .is_holiday = 1, .date_type = 1 }, "春节" },
    { .month = 1,  .day = 15, .flags = { .is_holiday = 0, .date_type = 1 }, "元宵节" },
    { .month = 1,  .day = 25, .flags = { .is_holiday = 0, .date_type = 1 }, "填仓节" },
    { .month = 5,  .day = 5,  .flags = { .is_holiday = 0, .date_type = 1 }, "端午节" },
    { .month = 7,  .day = 7,  .flags = { .is_holiday = 0, .date_type = 1 }, "七夕节" },
    { .month = 7,  .day = 15, .flags = { .is_holiday = 0, .date_type = 1 }, "中元节" },
    { .month = 8,  .day = 15, .flags = { .is_holiday = 1, .date_type = 1 }, "中秋节" },
    { .month = 9,  .day = 9,  .flags = { .is_holiday = 0, .date_type = 1 }, "重阳节" },
    { .month = 12, .day = 8,  .flags = { .is_holiday = 0, .date_type = 1 }, "腊八节" },
    { .month = 12, .day = 23, .flags = { .is_holiday = 0, .date_type = 1 }, "北方小年" },
    { .month = 12, .day = 24, .flags = { .is_holiday = 0, .date_type = 1 }, "南方小年" },
    { .month = 12, .day = 0,  .flags = { .is_holiday = 1, .date_type = 1 }, "除夕" },

    { .month = 5,  .day = 20, .flags = { .is_holiday = 0, .date_type = 2 }, "母亲节" },
    { .month = 6,  .day = 30, .flags = { .is_holiday = 0, .date_type = 2 }, "父亲节" },
    { .month = 11, .day = 44, .flags = { .is_holiday = 0, .date_type = 2 }, "感恩节" },
};

const FESTIVAL COMMEMORATION_DAY[] = {
    { .month = 1,  .day = 8,  .flags = { .is_holiday = 1, .date_type = 0 }, "周恩来逝世纪念日[1976]" },
    { .month = 5,  .day = 17, .flags = { .is_holiday = 0, .date_type = 0 }, "世界电信日[1969]" },
    { .month = 9,  .day = 9, .flags =  { .is_holiday = 0, .date_type = 0 }, "毛泽东逝世纪念日[1976]" },
    { .month = 12, .day = 26, .flags = { .is_holiday = 0, .date_type = 0 }, "毛泽东诞辰纪念日[1893]" },

    { .month = 2,  .day = 2,  .flags = { .is_holiday = 0, .date_type = 1 }, "龙抬头" },
    { .month = 9,  .day = 9,  .flags = { .is_holiday = 0, .date_type = 1 }, "中国老年节[1989]" },

    { .month = 4,  .day = 53, .flags = { .is_holiday = 0, .date_type = 3 }, "秘书节[1952]" },
};

/* MACROS */

/* GLOBAL FUNCTIONS */

/* Tells whether a year is leap year or not @param year a year @return 0: not leap year; 1: leap year */
uint8_t is_leap_year(int32_t year)
{
    return (year % 4) || ((year % 100 == 0) && (year % 400)) ? 0 : 1;
}

uint8_t get_month_days(int32_t year, uint8_t month)
{
    // assert(month > 0 && month <= 12); //, "set month between 1 and 12");

    return DAY_OF_MONTH[month-1] + (month == 2 && is_leap_year(year) ? 1 : 0) ;
}

uint8_t get_day_of_week(int32_t year, uint8_t month, uint8_t day)
{
#if 0
    uint32_t a = month < 3 ? 1 : 0;
    uint32_t b = year - a;

#if LV_CALENDAR_WEEK_STARTS_MONDAY
    uint32_t day_of_week = (day + (31 * (month - 2 + 12 * a) / 12) + b + (b / 4) - (b / 100) + (b / 400) - 1) % 7;
#else
    uint32_t day_of_week = (day + (31 * (month - 2 + 12 * a) / 12) + b + (b / 4) - (b / 100) + (b / 400)) % 7;
#endif

    return day_of_week;
#else
    if (month < 3)
    {
	    --year;
	    month += 12;
	}
	return (day + (13*month+8)/5 + year + (year/4) - (year/100) + (year/400)) % 7;
#endif
}

/* Get days elapsed from 1900/01/01 */
int32_t days_from_1900(int32_t year, uint8_t month, uint8_t day)
{
    if (year < 1900 || month > 12) return -1;

    uint32_t days = (year - 1900) * 365;
    if (year > 1904)
    {
        days += (year - 1901) / 4;
        if (year > 2100)
        {
            int n = year - 2001;
            days -= (n / 100);
            days += (n / 400);
        }
    }

    days += (DAY_OF_YEAR[month - 1] + day);
    if (!(month > 2 && is_leap_year(year)))
    {
        days -= 1;
    }
    return days;
}

/* Get seconds elapsed from 1900/01/01 00:00:00 */
int64_t seconds_from_1900(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (year < 1900 || month > 12) return -1;
    return days_from_1900(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
}

uint32_t solar_date_to_days(int32_t y, int m, int d)
{
    m = (m + 9) % 12;
    y = y - m / 10;
    return 365 * y + y / 4 - y / 100 + y / 400 + (m * 306 + 5) / 10 + (d - 1);
}

/* Convert days which elapsed from 0000/03/01 to a solar date g: days elapsed from 00/03/01 */
void days_to_solar_date(uint64_t g, LUNAR_DATE * solar_date)
{
    uint64_t y = (g * 10000 + 14780) / 3652425;
    int32_t ddd = g - (365 * y + y / 4 - y / 100 + y / 400);
    if (ddd < 0)
    {
        y--;
        ddd = g - (365 * y + y / 4 - y / 100 + y / 400);
    }

    int32_t mi = (100 * ddd + 52) / 3060;
    solar_date->year  = (int)(y + (mi + 2) / 12);
    solar_date->month = (int)((mi + 2) % 12 + 1);
    solar_date->day   = (int)(ddd - (mi * 306 + 5) / 10 + 1);
}

uint8_t get_leap_month(int32_t lunar_year)
{
    uint32_t monthInfo = lunar_month_days[lunar_year - lunar_month_days[0]];
    return (uint8_t)(monthInfo >> 13) & 0x0F;
    // return (uint8_t)(LUNAR_INFO[lunar_year - MIN_YEAR] >> 20) & 0x0F;
}

uint32_t get_lunar_year_days(int32_t year)
{
    uint32_t sum = get_leap_month(year) > 0 ? 377 : 348;

    uint32_t monthInfo = lunar_month_days[year - lunar_month_days[0]];
    // uint32_t monthInfo = LUNAR_INFO[year - MIN_YEAR] & 0x0FFF80;
    for (int32_t i = 0x1000; i > 0; i >>= 1)
    {
        if ((monthInfo & i))
            ++sum;
    }
    return sum;
}

static uint8_t _get_lunar_month_days(int32_t year, uint8_t month)
{
    return (lunar_month_days[year - lunar_month_days[0]] & (0x2000 >> month)) ? 30 : 29;
}

uint8_t get_lunar_month_days(int32_t year, uint8_t month, uint8_t is_leap)
{
    uint8_t leap_month = get_leap_month(year);
    if (leap_month && (month > leap_month || (is_leap && month == leap_month)))
    {
        month++;
    }

    return _get_lunar_month_days(year, month);
}

int getBitInt(int data, int length, int shift)
{
    return (data & (((1 << length) - 1) << shift)) >> shift;
}

int get_lunar_date(int32_t year, uint8_t month, uint8_t day, LUNAR_DATE * lunar_date)
{
    int index = year - solar_1_1[0];
    int data = (year << 9) | (month << 5) | day;
    int solar11;
    if (solar_1_1[index] > data)
    {
        index--;
    }
    solar11 = solar_1_1[index];
    int y = getBitInt(solar11, 12, 9);
    int m = getBitInt(solar11, 4, 5);
    int d = getBitInt(solar11, 5, 0);
    long offset = solar_date_to_days(year, month, day) - solar_date_to_days(y, m, d);

    int days = lunar_month_days[index];
    int leap = getBitInt(days, 4, 13);

    int lunarY = index + solar_1_1[0];
    int lunarM = 1;
    int lunarD;
    offset += 1;

    for (int i = 0; i < 13; i++)
    {
        int dm = getBitInt(days, 1, 12 - i) == 1 ? 30 : 29;
        if (offset > dm) {
            lunarM++;
            offset -= dm;
        } else {
            break;
        }
    }
    lunarD = (int) (offset);
    lunar_date->year = lunarY;
    lunar_date->month = lunarM;
    lunar_date->day = lunarD;

    lunar_date->ext.is_leap_month = 0;
    if (leap != 0 && lunarM > leap)
    {
        lunar_date->month -= 1;
        if (lunar_date->month == leap)
        {
            lunar_date->ext.is_leap_month = 1;
        }
    }
    return 0;
}

int lunarToSolar(int lunarYear, int lunarMonth, int lunarDay, uint8_t isLeap)
{
    int days = lunar_month_days[lunarYear - lunar_month_days[0]];
    int leap = getBitInt(days, 4, 13);
    int offset = 0;
    int loop = leap;
    if (!isLeap) {
        if (lunarMonth <= leap || leap == 0) {
            loop = lunarMonth - 1;
        } else {
            loop = lunarMonth;
        }
    }
    for (int i = 0; i < loop; i++) {
        offset += getBitInt(days, 1, 12 - i) == 1 ? 30 : 29;
    }
    offset += lunarDay;

    int solar11 = solar_1_1[lunarYear - solar_1_1[0]];

    int y = getBitInt(solar11, 12, 9);
    int m = getBitInt(solar11, 4, 5);
    int d = getBitInt(solar11, 5, 0);

    LUNAR_DATE solar_date;
    days_to_solar_date(solar_date_to_days(y, m, d) + offset - 1, &solar_date);

    return 0;
}

// month: 公历月份 day: 公历日期
const char * get_gre_festival(uint8_t month, uint8_t day)
{
    for (int i=0; i < sizeof(festivals)/sizeof(FESTIVAL); ++i)
    {
        const FESTIVAL * fes = &festivals[i];
        if (fes->flags.date_type == 0 && fes->month == month)
        {
            if (fes->day == day) // || (fes->day == 0 && day == DAY_OF_MONTH[month-1]))
            {
                return fes->festival;
            }
        }
    }

    return NULL;
}

// lunar_date: 农历日期
const char * get_lunar_festival(LUNAR_DATE * lunar_date)
{
    for (int i=0; i < sizeof(festivals)/sizeof(FESTIVAL); ++i)
    {
        const FESTIVAL * fes = &festivals[i];
        if (fes->flags.date_type == 1 && fes->month == lunar_date->month)
        {
            if (lunar_date->day == fes->day)
            {
                return fes->festival;
            }
            else if (fes->day == 0)
            {
                if (lunar_date->day == 30)
                {
                    return fes->festival;
                }
                else if (lunar_date->day == 29)
                {
                    if (get_lunar_month_days(lunar_date->year, lunar_date->month, lunar_date->ext.is_leap_month) == 29)
                    {
                        return fes->festival;
                    }
                }
            }
        }
    }
    return NULL;
}

const char * get_week_festival(uint8_t month, uint8_t day, uint8_t days_of_month, uint8_t week_of_day1)
{
#if 0
    char date[16];
	uint8_t advance = 0;
	uint8_t reverse = 0;
    uint8_t week = (day - 1 + week_of_day1) % 7;
    snprintf(date, sizeof(date), "%02d%d", month, week);
    for (int i=0; i < sizeof(WEEK_FESTIVAL)/sizeof(char *); ++i)
    {
        int n = strncmp(WEEK_FESTIVAL[i], date, 3);
        if (n == 0)
        {
            uint8_t no = WEEK_FESTIVAL[i][3] - '0';
			if (WEEK_FESTIVAL[i][4] == '+')
			{
				uint8_t first_sunday = week_of_day1 > 0 ? 8 - week_of_day1 : 1;
				uint8_t last_saturday = days_of_month - (days_of_month + week_of_day1) % 7;
				if (day <= last_saturday)
				{
					reverse = (last_saturday - day) / 7 + 1;
				}
				if (day >= first_sunday)
				{
					advance = (day - first_sunday) / 7 + 1;
				}
			}
			else
			{
				advance = (day + 6) / 7;
    			reverse = (days_of_month - day) / 7 + 1;
			}

			if (no == advance || no == (reverse + 4))
            {
                return WEEK_FESTIVAL[i];
            }
        }
        else if (n > 0)
        {
            break;
        }
    }
#else
    uint8_t advance = 0;
	uint8_t reverse = 0;
    uint8_t week = (day - 1 + week_of_day1) % 7;
    for (int i=0; i < sizeof(festivals)/sizeof(FESTIVAL); ++i)
    {
        const FESTIVAL * fes = &festivals[i];
        if (fes->flags.date_type > 1 && fes->month == month)       // 此处为公历月份
        {
            if (fes->day % 10 == week) // 比较星期
            {
                uint8_t no = fes->day / 10;
                if (fes->flags.date_type == 3)
                {
                    uint8_t first_sunday = week_of_day1 > 0 ? 8 - week_of_day1 : 1;
                    uint8_t last_saturday = days_of_month - (days_of_month + week_of_day1) % 7;
                    if (day <= last_saturday)
                    {
                        reverse = (last_saturday - day) / 7 + 1;
                    }
                    if (day >= first_sunday)
                    {
                        advance = (day - first_sunday) / 7 + 1;
                    }
                }
                else
                {
                    advance = (day + 6) / 7;
                    reverse = (days_of_month - day) / 7 + 1;
                }

                if (no == advance || no == (reverse + 4))
                {
                    return fes->festival;
                }
            }
        }
    }
#endif

    return NULL;
}

const char * get_animal_str(int32_t lunar_year)
{
    return ANIMALS[(lunar_year - 4) % 12];
}

const char * get_gan_str(int32_t lunar_year)
{
    return TIANGAN[(lunar_year - 4) % 10];
}

const char * get_zhi_str(int32_t lunar_year)
{
    return DIZHI[(lunar_year - 4) % 12];
}

const char * get_year_ganzhi_str(int32_t lunar_year)
{
    static char ganzhi[10];
    snprintf(ganzhi, sizeof(ganzhi), "%s%s", get_gan_str(lunar_year), get_zhi_str(lunar_year));
    return ganzhi;
}

const char * get_month_ganzhi_str(int32_t lunar_year, uint8_t lunar_month)
{
    static char ganzhi[10];
    

    uint8_t shift = ((lunar_year - 4) % 5) * 2 + 1;
    const char * gan = TIANGAN[(lunar_month + shift) % 10];
    const char * zhi = DIZHI[(lunar_month + 1) %12];
    snprintf(ganzhi, sizeof(ganzhi), "%s%s", gan, zhi);
    return ganzhi;
}

const char * get_day_ganzhi_str(int32_t gre_year, uint8_t month, uint8_t day)
{
    static char ganzhi[10];

    int32_t days = days_from_1900(gre_year, month, day);
    if (days >= 0)
    {
        days += 10;     // 1900/1/1为甲戌日(60进制10)
        snprintf(ganzhi, sizeof(ganzhi), "%s%s", TIANGAN[days%10], DIZHI[days%12]);
        return ganzhi;
    }
    return "";
}

const char * get_festival(int32_t year, uint8_t month, uint8_t day)
{
    uint8_t advance = 0;
	uint8_t reverse = 0;
    uint8_t days_of_month  = get_month_days(year, month);
    uint8_t week_of_day1 = get_day_of_week(year, month, 1);
    uint8_t week = (day - 1 + week_of_day1) % 7;
    for (int i=0; i < sizeof(festivals)/sizeof(FESTIVAL); ++i)
    {
        const FESTIVAL * fes = &festivals[i];
        if (fes->flags.date_type == 1)      // 比较农历月份和农历日期
        {
            if (fes->month == month)        // 此处为农历月份
            {
                if (fes->day == day || (fes->day == 0 && day == days_of_month))
                {
                    return fes->festival;
                }
            }
        }
        else if (fes->month == month)       // 此处为公历月份
        {
            if (fes->flags.date_type == 0)
            {
                if (fes->day == day || (fes->day == 0 && day == days_of_month))
                {
                    return fes->festival;
                }
            }
            else if (fes->day % 10 == week) // 比较星期
            {
                uint8_t no = fes->day / 10;
                if (fes->flags.date_type == 3)
                {
                    uint8_t first_sunday = week_of_day1 > 0 ? 8 - week_of_day1 : 1;
                    uint8_t last_saturday = days_of_month - (days_of_month + week_of_day1) % 7;
                    if (day <= last_saturday)
                    {
                        reverse = (last_saturday - day) / 7 + 1;
                    }
                    if (day >= first_sunday)
                    {
                        advance = (day - first_sunday) / 7 + 1;
                    }
                }
                else
                {
                    advance = (day + 6) / 7;
                    reverse = (days_of_month - day) / 7 + 1;
                }

                if (no == advance || no == (reverse + 4))
                {
                    return fes->festival;
                }
            }
        }
    }
    return NULL;
}

const char * get_lunar_month_str(uint8_t lunar_month, uint8_t is_leap_month)
{
    static char str[10];
    snprintf(str, sizeof(str), is_leap_month ? "闰%s%s" : "%s%s", MONTH_CN[lunar_month], MONTH_CN[0]);
    return str;
}

const char * get_lunar_day_str(uint8_t lunar_day)
{
    static char str[10];
    switch (lunar_day)
    {
        case 10:
            return "初十";
        case 20:
            return "二十";
        case 30:
            return "三十";
        default:
            snprintf(str, sizeof(str), "%s%s", DAY_TENS_DIGIT[lunar_day / 10], DAY_UNIT_DIGIT[lunar_day % 10]);
            break;
    }    
    return str;
}

const char * get_week_str(uint8_t week)
{
    return DAY_UNIT_DIGIT[week];
}

