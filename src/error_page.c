#include "error_page.h"

int error_page_get_new(server *srv, connection *con, buffer *errorpage)
{
	if (NULL == srv || NULL == con || NULL == errorpage)
	{
		return -1;
	}
	
	if (con -> http_status >= 200 && con -> http_status < 400)
	{
		log_error_write(srv, __FILE__, __LINE__, "s", "Seem no error...");
		return -1;
	}
	
	buffer_reset(errorpage);
	
	buffer_copy_string_len(errorpage, CONST_STR_LEN("<html>\n"" <head>\n" "  <title>"));
	
	buffer_append_string_len(errorpage, CONST_STR_LEN("ERROR - \n"));
	buffer_append_long(errorpage, con -> http_status);
	buffer_append_string_len(errorpage, CONST_STR_LEN("</title></head>\n"));
	buffer_append_string_len(errorpage, CONST_STR_LEN("<body>"));
	
	buffer_append_string_len(errorpage, CONST_STR_LEN("<h2>"));
	buffer_append_long(errorpage, con -> http_status);
	buffer_append_string_len(errorpage, CONST_STR_LEN("</h2>"));
	
	buffer_append_string_len(errorpage, CONST_STR_LEN("<h3>"));
	buffer_append_string(errorpage, get_http_status_name(con -> http_status));
	buffer_append_string_len(errorpage, CONST_STR_LEN("</h3>"));
	
	buffer_append_string_len(errorpage, CONST_STR_LEN("</br>"));
	buffer_append_string_len(errorpage, CONST_STR_LEN("</body>"));
	buffer_append_string_len(errorpage, CONST_STR_LEN("</html>"));
	
	return 0;
}
