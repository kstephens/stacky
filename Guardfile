guard :shell do
  watch(%r{src/.*\.c$}) { system("make") }
  watch(%r{include/.*\.h$}) { system("make") }
  watch(%r{Makefile$}) { system("make") }
end
