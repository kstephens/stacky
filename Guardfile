guard :shell do
  watch('src/*.c') { shell("make") }
  watch('include/**/*.h') { shell("make") }
  watch('Makefile') { shell("make") }
end
