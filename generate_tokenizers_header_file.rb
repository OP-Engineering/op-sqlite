require 'fileutils'

def generate_tokenizers_header_file(names, file_path)
  # Ensure the directory exists
  dir_path = File.dirname(file_path)
  FileUtils.mkdir_p(dir_path) unless Dir.exist?(dir_path)

  File.open(file_path, 'w') do |file|
    file.puts "#ifndef TOKENIZERS_H"
    file.puts "#define TOKENIZERS_H"
    file.puts
    file.puts "namespace opsqlite {"
    file.puts

    names.each do |name|
      file.puts "void #{name}();"
    end

    file.puts
    file.puts "} // namespace opsqlite"
    file.puts
    file.puts "#endif // TOKENIZERS_H"
  end
end

# # Example usage:
# names = ["functionOne", "functionTwo", "functionThree"]
# generate_header_file(names, "/path/to/generated_header.h")