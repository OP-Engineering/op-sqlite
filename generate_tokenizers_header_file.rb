require 'fileutils'

def generate_tokenizers_header_file(names, file_path, is_user_app)
  # Ensure the directory exists
  dir_path = File.dirname(file_path)
  FileUtils.mkdir_p(dir_path) unless Dir.exist?(dir_path)
  tokenizer_list = names.map { |name| "opsqlite_#{name}_init(db,&errMsg,nullptr);" }.join

  File.open(file_path, 'w') do |file|
    file.puts "#ifndef TOKENIZERS_H"
    file.puts "#define TOKENIZERS_H"
    file.puts
    file.puts "#define TOKENIZER_LIST #{tokenizer_list}"
    file.puts
    if is_user_app
      file.puts "#include \"sqlite3.h\""
    else
      file.puts "#include \"../../cpp/sqlite3.h\""
    end
    file.puts
    file.puts "namespace opsqlite {"
    file.puts

    names.each do |name|
      file.puts "int opsqlite_#{name}_init(sqlite3 *db, char **error, sqlite3_api_routines const *api);"
    end

    file.puts
    file.puts "} // namespace opsqlite"
    file.puts
    file.puts "#endif // TOKENIZERS_H"
  end
end