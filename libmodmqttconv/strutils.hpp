#include <algorithm>
#include <cctype>
#include <locale>

//based on https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring
class StrUtils {
    public:
        // trim from start (copying)
        static inline std::string ltrim(std::string s) {
            ltrimInPlace(s);
            return s;
        }

        // trim from end (copying)
        static inline std::string rtrim(std::string s) {
            rtrimInPlace(s);
            return s;
        }

        // trim from both ends (copying)
        static inline std::string trim(std::string s) {
            trimInPlace(s);
            return s;
        }
    private:
        // trim from start (in place)
        static inline void ltrimInPlace(std::string &s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
        }

        // trim from end (in place)
        static inline void rtrimInPlace(std::string &s) {
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
        }

        // trim from both ends (in place)
        static inline void trimInPlace(std::string &s) {
            rtrimInPlace(s);
            ltrimInPlace(s);
        }
};
