#include "htmlFilter.h"
#include "utils.h"
#include "../../include/debug.h"

HtmlFilter::HtmlFilter()
{
	_carryover_capacity = 0;
}
HtmlFilter::HtmlFilter(int carryover_len)
{
	_carryover_capacity = carryover_len;
}


// Specifies a set of html filters to be applied to the server response
// Returns true if any valid filters were processed
bool HtmlFilter::set_filter(const char* filter_spec)
{
	Debug_printf("HtmlFilter: reading filters: ");

	_html_filter_tags_disallowed.clear();
	_html_filter_attrs_disallowed.clear();
	_html_filter_tags_allowed.clear();
	_html_filter_attrs_allowed.clear();

	std::istringstream filters(filter_spec);
	std::string filter;
	while (std::getline(filters, filter, ';')) {
		util_string_tolower(filter);
		size_t pos = filter.find("=");
		if (pos > 0) {
			std::string filterName(filter.substr(0, pos));
			std::istringstream filterValues(filter.substr(pos + 1).c_str());
			std::string val;
			while (std::getline(filterValues, val, ',')) {
				util_string_trim(val);
				Debug_printf(" %s:%s", filterName.c_str(), val.c_str());

				if (filterName == "dt") {
					_html_filter_tags_disallowed.insert(val);

					// Special case, so <HTML> can be filtered out at start and end
					// if desired
					if (val == "html") {
						_html_filter_tags_disallowed.insert("/html");
					}
				}
				else if (filterName == "da") _html_filter_attrs_disallowed.insert(val);
				else if (filterName == "at") _html_filter_tags_allowed.insert(val);
				else if (filterName == "aa") _html_filter_attrs_allowed.insert(val);
				else if (filterName == "encoding" && val == "internal") _convert_to_internal_encoding = true;
			}
		}
	}

	// Flag we have one or more filters to apply
	int filter_count = _html_filter_tags_disallowed.size() + _html_filter_attrs_disallowed.size() +
		_html_filter_tags_allowed.size() + _html_filter_attrs_allowed.size();

	Debug_printf("\nHtmlFilter: filter count is %i\n", filter_count);

	if (_convert_to_internal_encoding) {
		Debug_printf("HtmlFilter: converting response to internal encoding\n");
	}

	return filter_count > 0;
}

void HtmlFilter::reset_state()
{
	_mode = FILTER_MODE_TEXT;
	_stop1 = '<';
	_stop2 = -1;
	_prev_char = ' ';
	_skip_tag = "";
	_tag = "";
	_attr = "";
	_skip_char = false;
	_in_pre = false; // <pre> still a WIP
	_self_closing_tag = false;
	_filter_attributes = _html_filter_attrs_allowed.size() > 0 ||
		_html_filter_attrs_disallowed.size() > 0;
}

int HtmlFilter::filter_chunk(char* buffer, int buffer_len)
{
	if (buffer_len == 0)
		return buffer_len;

	int dest_index = 0, start = -1, original_buffer_len = buffer_len;

	// If carrying over bytes from a previous chunk, prepend at the start
	// of the buffer, increasing the overall buffer size temporarilly,
	// then resume at that point in the buffer
	if (_carryover.length() > 0) {
		dest_index = _carryover.length();
		memmove(buffer + dest_index, buffer, buffer_len);
		memcpy(buffer, _carryover.c_str(), dest_index);
		_carryover = "";
		start = 0;
		buffer_len += dest_index;
	}
	else if (_mode == FILTER_MODE_TAG_NAME || _mode == FILTER_MODE_ATTR_NAME) {
		start = 0;
	}

	for (int i = dest_index; i < buffer_len; i++) {
		// Keep looping until a stop character is reached
		char c = buffer[i];

		// Treat \r\n\t as space

		if (!_in_pre && (c == 12 || c == 10 || c == 8))
			c = 0x20;

		// Write to new location in buffer if not skipping and not whitespace
		if (!_skip_char &&
			(
				(
					( // Avoid extra whitespace
						c != 0x20
						|| (
							_mode != FILTER_MODE_TEXT && (
								dest_index == 0
								|| (dest_index > 0 && buffer[dest_index - 1] != ' ' && buffer[dest_index - 1] != '>')
								)
							)
						|| _prev_char != 0x20
						) // Avoid non-legible characters
					&& c >= 0x20 && c <= 0x7C
					) || _in_pre // Allow any character in <pre>
				)
			) {
			buffer[dest_index++] = c;
			if (_mode == FILTER_MODE_TEXT && c != '<' && c != '>')
				_prev_char = c;
		}

		// Keep looping until a stop character is reached
		if (c != _stop1 && c != _stop2 && _stop1 != '*') {
			continue;
		}

		// Special case that does not fit - <!--...-->
		if (_stop1 == '*' && _mode == FILTER_MODE_TAG_NAME) {
			// if ! - - were found, continue doing this, else abort
			// to look for a general tag
			if (dest_index - start < 5) {
				if (c == _stop2) {
					_stop2 = '-';
					continue;
				}
				else {
					_stop1 = '>'; _stop2 = ' ';
					if (c != _stop1 && c != _stop2)
						continue;
				}
			}
			else {
				// If we have made it this far, the normal logic will
				// read the tag as !--, and skip to the end since the
				// character is a space
				c = ' ';
			}
		}

		// Set the default stop characters for the next mode
		// Will be overridden if needed
		_stop1 = '<'; _stop2 = -1;

		// Decide what to do next based on the mode
		switch (_mode) {
		case FILTER_MODE_TEXT:
			_mode = FILTER_MODE_TAG_NAME;

			// We start allowing any character just in case we are at the start of <!--
			_stop1 = '*'; _stop2 = '!';

			// If the char was skipped, it did not get added to the buffer, so add it now.
			if (_skip_char) {
				buffer[dest_index++] = c;
			}
			_skip_char = false;
			start = dest_index - 1;

			// start < 0 shouldn't happen, leave breakpoint for more test scenarios
			// before removing this
			if (start < 0) {
				start = 0;
				Debug_printf("HtmlFilter: WARNING: start is %i for at char %c", start, c);
			}
			break;
		case FILTER_MODE_SKIP_TO_TAG_END:

			if (!_self_closing_tag) {
				if (c == '/') {
					// Found a /. The next char must be a > to be a self closing tag
					_stop1 = '*';
					continue;
				}

				if (_stop1 == '*') {
					// Second char is > afer a /, so we know it is self closing
					if (c == '>') {
						_self_closing_tag = true;
					}
					else {
						// Keep skipping until the end tag is found
						_stop1 = '>';
						_stop2 = '/';
						continue;
					}
				}
			}

			if (!_self_closing_tag && _skip_tag.length() == 0) {
				_skip_tag = "/" + _tag;
			}
			_skip_char = !_self_closing_tag;
			_mode = FILTER_MODE_TEXT;
			break;

		case FILTER_MODE_TAG_NAME:
			_tag = std::string(&buffer[start + 1], dest_index - start - 2);
			util_string_tolower(_tag);

			_in_pre = _tag == "pre";
			if (_skip_tag.length() > 0) {
				if (_skip_tag == _tag) {
					_skip_tag = "";
					_skip_char = false;
					_self_closing_tag = true;
				}
				else {
					_skip_char = true;
				}

				// Roll back this tag as we are actively skipping
				dest_index = start;
				start = -1;

				if (c == ' ') {
					// Not the end of the tag? skip ahead until a > is found
					_mode = FILTER_MODE_SKIP_TO_TAG_END;
					_skip_char = true;
					_stop1 = '>'; _stop2 = '/';
				}
				else {
					_mode = FILTER_MODE_TEXT;
				}
			}
			else if (_tag.length() > 0) {
				_self_closing_tag = false;

				// Remove trailing / on self closing tags
				if (_tag[_tag.length() - 1] == '/') {
					_tag = _tag.substr(0, _tag.length() - 1);
					_self_closing_tag = true;
				}

				// Should we skip the tag?
				if (_html_filter_tags_disallowed.count(_tag) || (
					_html_filter_tags_allowed.size() > 0 &&
					_html_filter_tags_allowed.count(_tag) == 0)
					) {

					Debug_printf(" <%s>", _tag.c_str());

					// Roll back dest index to start of the tag
					dest_index = start;
					start = -1;

					if (_tag == "!doctype" || _tag == "meta" || _tag == "link" || _tag == "img"
						|| _tag == "br" || _tag == "html" || _tag == "/html" || _tag == "!--") {
						_self_closing_tag = true;
					}

					// If we stopped on a space instead of >, skip ahead to find out if it is self closing
					if (c == ' ') {
						_mode = FILTER_MODE_SKIP_TO_TAG_END;
						_skip_char = true;
						_stop1 = '>';

						if (!_self_closing_tag) {
							// If self closing is unknown, search for /> just in case
							_stop2 = '/';
						}
					}
					else {
						// We stopped on > (end of tag)
						_mode = FILTER_MODE_TEXT;
						if (!_self_closing_tag) {
							_skip_tag = "/" + _tag;
						}
						_skip_char = !_self_closing_tag;
					}
				}
				else {

					//Not skipping, so proceed to find attributes if not at the end
					if (_mode == FILTER_MODE_TAG_NAME && _filter_attributes && c == ' ') {
						_mode = FILTER_MODE_ATTR_NAME;
						_stop1 = '='; _stop2 = '>';
						start = dest_index - 1;
						if (start < 0) {
							start = 0;
							Debug_printf("HtmlFilter: WARNING: start is %i for tag %s at char %c", start, _tag.c_str(), c);
						}
					}
					else {
						_mode = FILTER_MODE_TEXT;
					}

				}
			}
			break;
		case FILTER_MODE_ATTR_NAME:
			if (c == '=' && dest_index > start + 1) {
				_attr = std::string(&buffer[start], dest_index - start - 1);
				util_string_trim(_attr);
				util_string_tolower(_attr);

				// Should we skip the attribute?
				if (_html_filter_attrs_disallowed.count(_attr) || (
					_html_filter_attrs_allowed.size() > 0 &&
					_html_filter_attrs_allowed.count(_attr) == 0)
					) {
					Debug_printf(" %s", _attr.c_str());

					// Roll back dest index to the start of the attribute
					dest_index = start;
					start = -1;
					_skip_char = true;
				}

				_mode = FILTER_MODE_FIND_ATTR_VAL_STOP;
				_stop1 = '*';
			}
			else {
				_mode = FILTER_MODE_TEXT;
			}
			break;
		case FILTER_MODE_FIND_ATTR_VAL_STOP:
			// find the stop character for the attr value, either ' or " or
			// a space if characters start without a quote in front 
			if (c == ' ') {
				continue;
			}
			_mode = FILTER_MODE_ATTR_VAL;
			_stop1 = (c == '"' || c == '\'') ? c : ' ';
			_stop2 = '>';
			break;
		case FILTER_MODE_ATTR_VAL:
			// We've reached the end of this value, so we can stop skipping chars
			if (_skip_char) {
				_skip_char = false;

				// If the closing > was skipped, we need to write it out
				if (c == '>') {
					buffer[dest_index++] = c;
				}
			}

			// Search for the next attribute
			if (c != '>') {
				_mode = FILTER_MODE_ATTR_NAME;
				_stop1 = '='; _stop2 = '>';
				start = dest_index;
				if (start < 0) {
					start = 0;
					Debug_printf("\nHtmlFilter: WARNING: start is %i for tag %s at char %c", start, _tag.c_str(), c);
				}
			}
			else {
				// Tag is at an end. Move on.
				_mode = FILTER_MODE_TEXT;
			}
			break;
		}

		// Trim space before closing tag (e.g. "<br />" -> "<br/>"
		if (_mode != FILTER_MODE_ATTR_VAL && dest_index > 3) {
			for (int j = 0; j < 5; j++) {
				if (dest_index > 3 &&
					buffer[dest_index - 1] == '>' &&
					buffer[dest_index - 2] == '/' &&
					buffer[dest_index - 3] == ' ') {
					buffer[dest_index - 3] = buffer[dest_index - 2];
					buffer[dest_index - 2] = buffer[dest_index - 1];
					dest_index--;
				}
				else break;
			}
		}
	}

	// Store the start a token to carry over to the beginning of the next chunk
	if (start > -1 && _carryover_capacity > 0 && (
		_mode == FILTER_MODE_TAG_NAME || _mode == FILTER_MODE_ATTR_NAME
		)) {
		_carryover = std::string(buffer + start, dest_index - start);
		if (_carryover.length() > _carryover_capacity) {
			_carryover = _carryover.substr(0, _carryover_capacity);
		}
		dest_index = start;
	}

	buffer_len = dest_index;

	if (buffer_len != original_buffer_len) {
#ifdef VERBOSE_HTTP
		Debug_printf("HtmlFilter: Change chunk length from %i to %i\n", original_buffer_len, buffer_len);
#endif
	}

	// Convert to internal encoding if requested
	if (_convert_to_internal_encoding) {
		for (int i = buffer_len - 1; i >= 0; i--) {
			char c = buffer[i];
			buffer[i] = c - (32 * (c < 96)) + (96 * (c < 32));
			if (c == 0x60) buffer[i] = 7; // ' char
		}
	}

	return buffer_len;
}