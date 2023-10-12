

def get_print_title_distance(title_unity):
    title_list = title_unity.split(' ')
    title_index_list = []
    title_old_index = 0
    for title in title_list:
        if title:
            y = title_unity.index(title)
            if title_index_list:
                title_index_list.append(y-title_old_index)
            else:
                title_index_list.append(y)
            title_old_index = y
    title_list = [val for val in title_list if val]
    title_print_distance_str='{:<' + '}{:<'.join([str(i) for i in title_index_list[1:]]) + '}{:<'+(str(title_index_list[-1])) + '}'
    return title_print_distance_str