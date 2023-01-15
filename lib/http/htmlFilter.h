/* Html Parser
 Accepts a string containing directives to specify which tags and attributes to
 remove. Whitespace is also removed, preserving a minimum of one space between elements
 to support spacing between elements.

 If you work primarily in memory, you can also specify encoding to be INTERNAL.

 Format:
 [Directive 1]=[Value 1],[Value 2],..[Value N];[Directive 2]=[Value 1],..

 Case is ignored.

 Accepted Directives:
  DT - Disallow Tags - Specified tags are removed, all others preserved.
  DA - Disallow Attributes - Specified attributes are removed, all others preserved.
  ENCODING - Specify encoding. Right now the only accepted value is "internal"

 Examples: 
    encoding=Internal;dt=!--,PRE,!DOCTYPE,HTML,SCRIPT,NOSCRIPT,STYLE,META,LINK,SVG,IFRAME;AA=ID,TITLE,ALT,HREF,ARIA-LABEL,ACTION,METHOD,NAME,TYPE,VALUE"


Future Directives (work in progress):
  If needed, may expand to 
  AT - Allow Tags - Specified tags are preserved, all others removed.
  AA - Allow Attributes - Specified attributes are preserved, all others removed.
*/

#ifndef HTMLFILTER_H
#define HTMLFILTER_H

#include <string>
#include <map>
#include <sstream>
#include <set>

class HtmlFilter
{
private:
    enum HtmlFilterMode
    {
        FILTER_MODE_TEXT,
        FILTER_MODE_TAG_NAME,
        FILTER_MODE_ATTR_NAME,
        FILTER_MODE_FIND_ATTR_VAL_STOP,
        FILTER_MODE_ATTR_VAL,
        FILTER_MODE_SKIP_TO_TAG_END
    };

    std::set<std::string> _html_filter_tags_allowed;
    std::set<std::string> _html_filter_tags_disallowed;
    std::set<std::string> _html_filter_attrs_allowed;
    std::set<std::string> _html_filter_attrs_disallowed;

    bool _filter_attributes = false;
    bool _convert_to_internal_encoding = false;

    int _carryover_capacity;
    std::string _carryover;
    
    HtmlFilterMode _mode;
    int _stop1, _stop2; // Stop characters
    std::string _skip_tag; // Current tag being skipped
    std::string _tag; // Placeholder for tag
    std::string _attr; // Placeholder for attribute
    bool _skip_char; // Index to start skipping
    bool _in_pre; // If in PRE tag, do not filter out whitespace
    bool _self_closing_tag; // true if the tag is self closing r.g. <br/>
    char _prev_char; // Previous text character, used to reduces spaces

public:
    HtmlFilter();
    HtmlFilter(int carryover_len);

    void reset_state();
    int filter_chunk(char* buffer, int len);
    bool set_filter(const char* html_filter);
};


#endif // HTMLFILTER_H