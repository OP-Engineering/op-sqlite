require 'fileutils'

def generate_tokenizers_header_file(names, file_path, is_user_app)
  # Ensure the directory exists
  dir_path = File.dirname(file_path)
  FileUtils.mkdir_p(dir_path) unless Dir.exist?(dir_path)

  File.open(file_path, 'w') do |file|
    file.puts "#ifndef TOKENIZERS_H"
    file.puts "#define TOKENIZERS_H"
    file.puts
    if is_user_app
      file.puts "#include \"node-modules/@op-engineering/op-sqlite/cpp/sqlite3ext.h\""
    else
      file.puts "#include \"../../cpp/sqlite3.h\""
    end
    file.puts
    file.puts "namespace opsqlite {"
    file.puts

    names.each do |name|
      file.puts "int opsqlite_#{name}_init(sqlite3 *db, char **error, const sqlite3_api_routines *api);"
    end

    file.puts
    file.puts "} // namespace opsqlite"
    file.puts
    file.puts "#endif // TOKENIZERS_H"
  end
end