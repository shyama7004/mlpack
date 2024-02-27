#!/usr/bin/env bash
#
# Convert all the Markdown files in doc/ to HTML.
# This requires `kramdown` to be available on the path.
# `tidy` and `checklink` (from Debian's w3c-linkchecker package) are used to
# test the output and must also be available and on the path.
# Run this from the root directory of the repository.

template_html_header=doc/html/template.html.header;
template_html_footer=doc/html/template.html.footer;

if ! command -v kramdown &>/dev/null
then
  echo "kramdown not installed!  Cannot build documentation.";
  exit 1;
fi

if ! command -v tidy &>/dev/null
then
  echo "tidy not installed!  Cannot build documentation.";
  exit 1;
fi

if ! command -v checklink &>/dev/null
then
  echo "checklink not installed!  Cannot build documentation.";
  exit 1;
fi

if [ ! -d doc/ ];
then
  echo "Run this script from the root of the mlpack repository.";
  exit 1;
fi

# Define utility function to run kramdown and turn an .md file to an .html file.
run_kramdown()
{
  input_file=$1;
  # This converts, e.g., ./doc/user/index.md -> doc/html/user/index.html.
  tmp=${input_file#./doc/}; # Strip leading ./doc/.
  output_file=doc/html/${tmp%.md}.html;

  # Determine what the link root is.  If we're in the root directory, it's
  # nothing, otherwise it's one of more '../'s.
  dir_name=$(dirname $tmp);
  link_root="";
  if [[ "$dir_name" != "." ]];
  then
    levels_below_root=`echo $dir_name | awk -F'/' '{ print NF }'`;
    link_root=$(printf '../%.0s' `seq 1 $levels_below_root`);
  fi

  # Make the enclosing directory if needed.
  mkdir -p $(dirname $output_file);

  # Kramdown doesn't detect languages correctly with the "```" fence; instead it
  # needs the "~~~" fence.
  sed 's/^```/~~~/' $input_file > $input_file.tmp;

  # Our documentation is full of relative links, like
  # [name](other_file.md#anchor).  We need these to turn into links to the
  # rendered HTML file, like [name](other_file.html#anchor).  We'll do this with
  # regular expressions...
  #
  # - Note that this assumes there are no spaces in any filenames.
  # - We also only catch the second part of the link '](' because the name of
  #   the link could be spread on multiple lines.
  #
  # We start by trying to catch a special case of README.md, which our
  # documentation puts in a slightly different place.
  sed -i "s|\]([./]*README.md)|${link_root}README.html)|g" $input_file.tmp;
  sed -i "s|\]([./]*README.md#\([^ ]*\))|${link_root}README.html#\1)|g" $input_file.tmp;
  sed -i 's/\](\([^ ]*\).md)/](\1.html)/g' $input_file.tmp;
  sed -i 's/\](\([^ ]*\).md#\([^ ]*\))/](\1.html#\2)/g' $input_file.tmp;

  # Replace any links to source files with a link to the current version of the
  # source file on Github.
  sed -i 's/\](\/src\/\([^ ]*\)\.hpp)/](https:\/\/github.com\/mlpack\/mlpack\/blob\/master\/src\/\1.hpp)/' $input_file.tmp;

  kramdown \
      -x parser-gfm \
      --syntax-highlighter rouge \
      --syntax-highlighter-opts '{ default_lang: c++ }' \
      --auto_ids \
      $input_file.tmp > $output_file.tmp || exit 1;
  cat $template_html_header | sed "s|LINKROOT|$link_root|" > $output_file;

  # Add clickable anchors to h2 and h3 headers.
  sed -E 's/<h([23]) id="([^"]*)">/<h\1 id="\2"><a href="#\2" class="pl">🔗<\/a> /' $output_file.tmp >> $output_file;

  # Simple postprocessing to make tidy a little happier.
  # (Muting the warning won't change the error code!)
  sed -i 's/<table>/<table summary="">/' $output_file;

  cat $template_html_footer >> $output_file;
  rm -f $input_file.tmp $output_file.tmp;
}

# Create the template header file.
create_template_header()
{
  output_file=$1;

  # Note that LINKROOT will be substituted into place by run_kramdown.
  cat > $output_file << EOF
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
  <meta content="text/html; charset=utf-8" http-equiv="Content-Type">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <link type="text/css" rel="stylesheet" href="LINKROOTgfm-mod.css">
  <link type="text/css" rel="stylesheet" href="LINKROOTrouge-github-mod.css">
  <title>mlpack documentation</title>
</head>
<body>
EOF
}

# Create the template footer.
create_template_footer()
{
  output_file=$1;

  cat > $output_file << EOF
</body>
</html>
EOF
}

rm -rf doc/html/;
mkdir -p doc/html/;
cp doc/js/* doc/html/;
mkdir -p doc/html/user/img/;
cp doc/img/* doc/html/user/img/;
mkdir -p doc/html/tutorials/res/;
cp doc/tutorials/res/* doc/html/tutorials/res/;

# Create the template files we will use.
create_template_header $template_html_header;
create_template_footer $template_html_footer;

# Process all the .md files.
for f in README.md `find ./doc/ -iname '*.md'`;
do
  # Skip the JOSS paper...
  if [[ $f == *"joss_paper"* ]]; then
    continue;
  fi

  echo "Processing $f...";
  run_kramdown $f;

  # This converts, e.g., ./doc/user/index.md -> doc/html/user/index.html.
  tmp=${f#./doc/}; # Strip leading ./doc/.
  of=doc/html/${tmp%.md}.html;

  tidy -qe $of || exit 1;
done

# Now take a second pass to check all the links.
for f in `find ./doc/html/ -iname '*.html'`;
do
  echo "Checking links in $f...";

  # To run checklink we have to strip out some perl stderr warnings...
  checklink -qs --follow-file-links --suppress-broken 405 $f 2>&1 |
      grep -v 'Use of uninitialized value' > checklink_out;
  if [ -s checklink_out ];
  then
    cat checklink_out;
    exit 1;
  fi
  rm -f checklink_out;
done

# Remove temporary files.
rm -f $template_html_header;
rm -f $template_html_footer;
