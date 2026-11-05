#pragma once
#include <time.h>
#include <stdint.h>

class ClockCalendar final {
public:
    static bool leap(int year) { return year%4==0 && (year%100!=0 || year%400==0); }
    static int monthDays(int year,int month) {
        static constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
        if(month<1 || month>12)return 0;
        return month==2 && leap(year)?29:days[month-1];
    }
    static bool toUtc(int year,int month,int day,int hour,int minute,int second,time_t& epoch) {
        if(year<1970 || year>2099 || month<1 || month>12 || day<1 || day>monthDays(year,month) ||
            hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59)return false;
        int64_t days=0;
        for(int y=1970;y<year;++y)days+=leap(y)?366:365;
        for(int m=1;m<month;++m)days+=monthDays(year,m);
        days+=day-1;epoch=static_cast<time_t>(days*86400+hour*3600+minute*60+second);return true;
    }
    static bool validNetworkEpoch(int64_t epoch) { return epoch>=946684800LL && epoch<=4102444799LL; }
};
