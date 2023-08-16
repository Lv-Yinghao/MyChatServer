# 将指定路径下所有原文件名字放到变量名SRC_LIST中
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./redis REDIS_LIST)

# 表示生成可执行文件server，由SRC_LIST变量所定义的源文件编译
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${REDIS_LIST})

# 表示需要链接的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)

