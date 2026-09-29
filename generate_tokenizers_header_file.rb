require 'shellwords'

# Thin wrapper around generate_tokenizers_header_file.js, which holds the actual
# generation logic so CocoaPods, SwiftPM and Gradle all emit the exact same
# header. Node is already a hard requirement for building a React Native app.
def generate_tokenizers_header_file(names, file_path)
  script_path = File.join(__dir__, "generate_tokenizers_header_file.js")
  # NODE_BINARY is React Native's own convention for pointing at a specific
  # node (see the .xcode.env files it generates).
  node = ENV["NODE_BINARY"]
  node = "node" if node.nil? || node.empty?

  command = [node, script_path, file_path, *names]

  unless system(*command)
    raise "[OP-SQLITE] Could not generate the tokenizers header. Command: #{command.shelljoin}. " \
          "Make sure node is on your PATH or set NODE_BINARY to its location."
  end
end
