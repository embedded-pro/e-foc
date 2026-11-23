-- Pandoc filter for the LaTeX renders of the booklet and the document PDFs.
-- Inline code gets break points, so a long path or name wraps instead of running into the next table column.
-- Wide tables get widths from their content, not from the source's dashes, so their cells wrap within the text width;
-- every cell is ragged-right and can hyphenate its first word, which TeX otherwise never does.

local BREAK_AFTER = { ["/"] = true, ["."] = true, ["_"] = true, [":"] = true, ["-"] = true, [","] = true,
                      ["="] = true, ["("] = true, ["<"] = true }
local ESCAPES = { ["\\"] = "\\textbackslash{}", ["{"] = "\\{", ["}"] = "\\}", ["$"] = "\\$", ["&"] = "\\&",
                  ["#"] = "\\#", ["%"] = "\\%", ["_"] = "\\_", ["^"] = "\\^{}", ["~"] = "\\textasciitilde{}",
                  ["<"] = "\\textless{}", [">"] = "\\textgreater{}" }
local NATURAL_WIDTH = 72
local MAX_COLUMN = 48
local MIN_COLUMN = 6

local function breakable(text)
  local out = {}
  local previous = ""
  for _, code in utf8.codes(text) do
    local char = utf8.char(code)
    if char:match("^%u$") and previous:match("^%l$") then
      out[#out + 1] = "\\allowbreak{}"
    end
    out[#out + 1] = ESCAPES[char] or char
    if BREAK_AFTER[char] then
      out[#out + 1] = "\\allowbreak{}"
    end
    previous = char
  end
  return table.concat(out)
end

local function code_inline(code)
  return pandoc.RawInline("latex", "\\texttt{" .. breakable(code.text) .. "}")
end

local function rows_of(tbl)
  local rows = {}
  for _, row in ipairs(tbl.head.rows) do rows[#rows + 1] = row end
  for _, body in ipairs(tbl.bodies) do
    for _, row in ipairs(body.head) do rows[#rows + 1] = row end
    for _, row in ipairs(body.body) do rows[#rows + 1] = row end
  end
  for _, row in ipairs(tbl.foot.rows) do rows[#rows + 1] = row end
  return rows
end

local function longest_word(text)
  local longest = 0
  for word in text:gmatch("%S+") do
    longest = math.max(longest, utf8.len(word) or #word)
  end
  return longest
end

local function content_widths(tbl, rows)
  local widths, words = {}, {}
  for i = 1, #tbl.colspecs do widths[i], words[i] = 0, 0 end
  for _, row in ipairs(rows) do
    for i, cell in ipairs(row.cells) do
      if i <= #widths then
        local text = pandoc.utils.stringify(cell.contents)
        widths[i] = math.max(widths[i], utf8.len(text) or #text)
        words[i] = math.max(words[i], longest_word(text))
      end
    end
  end
  return widths, words
end

-- A column is never narrower than its longest word, short of MAX_COLUMN, so a word is not split in two.
local function set_widths(tbl, widths, words)
  local natural, capped = 0, 0
  for i, width in ipairs(widths) do
    natural = natural + width + 3
    local floor = math.max(MIN_COLUMN, math.min(MAX_COLUMN, words[i] + 2))
    widths[i] = math.max(floor, math.min(MAX_COLUMN, width))
    capped = capped + widths[i]
  end
  if natural <= NATURAL_WIDTH then
    for i, spec in ipairs(tbl.colspecs) do tbl.colspecs[i] = { spec[1], pandoc.ColWidthDefault } end
    return
  end
  for i, spec in ipairs(tbl.colspecs) do
    tbl.colspecs[i] = { spec[1], widths[i] / capped }
  end
end

local function ragged(cell)
  local first = cell.contents[1]
  if first and (first.t == "Plain" or first.t == "Para") then
    first.content:insert(1, pandoc.RawInline("latex", "\\RaggedRight\\hspace{0pt}"))
  end
end

local function table_block(tbl)
  local rows = rows_of(tbl)
  local widths, words = content_widths(tbl, rows)
  set_widths(tbl, widths, words)
  for _, row in ipairs(rows) do
    for _, cell in ipairs(row.cells) do ragged(cell) end
  end
  return tbl
end

-- unicode-math claims the prime character, so a prime in running text becomes a math prime.
local function prime_inline(str)
  if not str.text:find("′", 1, true) then return nil end
  local parts = pandoc.List()
  local first = true
  for piece in (str.text .. "′"):gmatch("(.-)′") do
    if not first then parts:insert(pandoc.RawInline("latex", "\\ensuremath{{}^{\\prime}}")) end
    if piece ~= "" then parts:insert(pandoc.Str(piece)) end
    first = false
  end
  return parts
end

-- Tables first: their widths are measured on the code text before it becomes raw LaTeX.
return {
  { Table = table_block },
  { Code = code_inline, Str = prime_inline },
}
