require 'rubygems'
require 'C4'

INPUT_DIR="input"
OUTPUT_DIR="output"
EXPECTED_DIR="expected"

def do_test(c4)
  puts "===="
  tests = Dir.entries(INPUT_DIR).reject { |i| i.match(/^\./) }
  tests.each do |test|
    print "Running test \"#{test}\"..."
    input = ""
    output = ""
    File.open("#{INPUT_DIR}/#{test}").each_line do |line|
      if line =~ /^\\dump (.+)/
        c4.install_str(input) unless input == ""
        output << "**** \\dump \"#{$1}\" ****\n"
        output << c4.dump_table($1).split("\n").sort.join("\n")
        output << "\n"
        input = ""
      else
        input << line
      end
    end
    puts " done"

    Dir.mkdir(OUTPUT_DIR) unless File.directory?(OUTPUT_DIR)
    File.open("#{OUTPUT_DIR}/#{test}", 'w') do |f|
      f.write(output)
    end
  end

  puts "===="
  `diff -ur #{EXPECTED_DIR} #{OUTPUT_DIR} > regress.diffs`
  if $? == 0
    puts "All tests passed!"
  else
    puts "TESTS FAILED; see regress.diffs"
  end
end

c = C4.new
do_test(c)
c.destroy
