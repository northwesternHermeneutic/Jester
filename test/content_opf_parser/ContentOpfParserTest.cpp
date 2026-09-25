#include <gtest/gtest.h>

#include <string>

#include "ContentOpfParser.h"

namespace {

void parse(ContentOpfParser& parser, const std::string& xml) {
  ASSERT_TRUE(parser.setup());
  EXPECT_EQ(parser.write(reinterpret_cast<const uint8_t*>(xml.data()), xml.size()), xml.size());
}

}  // namespace

TEST(ContentOpfParserMetadata, EntityCallbackDoesNotSplitOneAuthor) {
  const std::string xml =
      R"(<package xmlns:dc="urn:dc"><metadata><dc:creator>&#201;mile Zola</dc:creator></metadata></package>)";
  ContentOpfParser parser("", "", xml.size(), nullptr);

  parse(parser, xml);

  EXPECT_EQ(parser.author, "Émile Zola");
}

TEST(ContentOpfParserMetadata, ClampsOversizedMetadataTextInsteadOfGrowingUnbounded) {
  const std::string hugeTitle(64 * 1024, 'A');
  const std::string xml =
      "<package xmlns:dc=\"urn:dc\"><metadata><dc:title>" + hugeTitle + " tail</dc:title></metadata></package>";
  ContentOpfParser parser("", "", xml.size(), nullptr);

  parse(parser, xml);

  EXPECT_EQ(parser.title.size(), 512u);
  EXPECT_EQ(parser.title[0], 'A');
}

TEST(ContentOpfParserMetadata, ClampDoesNotSplitUtf8Codepoints) {
  const std::string prefix(511, 'A');
  const std::string xml = "<package xmlns:dc=\"urn:dc\"><metadata><dc:title>" + prefix +
                          "\xC3\xA9&amp;tail</dc:title></metadata></package>";
  ContentOpfParser parser("", "", xml.size(), nullptr);

  parse(parser, xml);

  EXPECT_EQ(parser.title, prefix);
}

TEST(ContentOpfParserMetadata, ClampNeverOvershootsAtAMultiCreatorSeparatorBoundary) {
  // First creator fills the field to exactly one byte under the cap, so the
  // second creator's leading ", " separator is the thing that would push the
  // total past 512 if the clamp checked only the next character.
  const std::string firstAuthor(511, 'A');
  const std::string xml = "<package xmlns:dc=\"urn:dc\"><metadata><dc:creator>" + firstAuthor +
                          "</dc:creator><dc:creator>B</dc:creator></metadata></package>";
  ContentOpfParser parser("", "", xml.size(), nullptr);

  parse(parser, xml);

  EXPECT_LE(parser.author.size(), 512u);
  EXPECT_EQ(parser.author, firstAuthor);
}

TEST(ContentOpfParserMetadata, SeparatesCreatorElementsAndCollapsesXmlWhitespace) {
  const std::string xml = R"(<package xmlns:dc="urn:dc"><metadata>
    <dc:title>  The
   Left Hand   of Darkness  </dc:title>
    <dc:creator> Ursula   K. Le Guin </dc:creator>
    <dc:creator>
Octavia E. Butler
</dc:creator>
  </metadata></package>)";
  ContentOpfParser parser("", "", xml.size(), nullptr);

  parse(parser, xml);

  EXPECT_EQ(parser.title, "The Left Hand of Darkness");
  EXPECT_EQ(parser.author, "Ursula K. Le Guin, Octavia E. Butler");
}

TEST(ContentOpfParserMetadata, ReadsCalibreSeriesAndFirstSubject) {
  const std::string xml = R"(<package xmlns:dc="urn:dc"><metadata>
    <meta content="Earthsea" name="calibre:series"/>
    <dc:subject> Fantasy &amp; Adventure </dc:subject>
    <dc:subject>Young adult</dc:subject>
  </metadata></package>)";
  ContentOpfParser parser("", "", xml.size(), nullptr);
  parse(parser, xml);
  EXPECT_EQ(parser.series, "Earthsea");
  EXPECT_EQ(parser.subject, "Fantasy & Adventure");
}

TEST(ContentOpfParserMetadata, DistinguishesEpubThreeSeriesFromSets) {
  const std::string xml = R"(<package><metadata>
    <meta property="belongs-to-collection" id="collection">Earthsea</meta>
    <meta refines="#collection" property="collection-type">series</meta>
  </metadata></package>)";
  ContentOpfParser parser("", "", xml.size(), nullptr);
  parse(parser, xml);
  EXPECT_EQ(parser.series, "Earthsea");

  const std::string setXml = R"(<package><metadata>
    <meta property="belongs-to-collection" id="collection">Boxed books</meta>
    <meta refines="#collection" property="collection-type">set</meta>
  </metadata></package>)";
  ContentOpfParser setParser("", "", setXml.size(), nullptr);
  parse(setParser, setXml);
  EXPECT_TRUE(setParser.series.empty());

  const std::string mixedXml = R"(<package><metadata>
    <meta property="belongs-to-collection" id="set">Boxed books</meta>
    <meta refines="#set" property="collection-type">set</meta>
    <meta property="belongs-to-collection" id="series">Earthsea</meta>
    <meta refines="#series" property="collection-type">series</meta>
  </metadata></package>)";
  ContentOpfParser mixedParser("", "", mixedXml.size(), nullptr);
  parse(mixedParser, mixedXml);
  EXPECT_EQ(mixedParser.series, "Earthsea");
}

TEST(ContentOpfParserMetadata, StopsBeforeManifestWithoutOpeningTemporaryStorage) {
  const std::string xml = R"(<package xmlns:dc="urn:dc"><metadata>
    <dc:title>A Wizard of Earthsea</dc:title>
    <dc:creator>Ursula K. Le Guin</dc:creator>
    <dc:language>en</dc:language>
  </metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest>
  </package>)";
  Storage = {};
  ContentOpfParser parser("/missing-cache", "OPS/", xml.size(), nullptr, /*collectCssFiles=*/true,
                          /*metadataOnly=*/true);

  ASSERT_TRUE(parser.setup());
  EXPECT_LT(parser.write(reinterpret_cast<const uint8_t*>(xml.data()), xml.size()), xml.size());
  EXPECT_EQ(parser.title, "A Wizard of Earthsea");
  EXPECT_EQ(parser.author, "Ursula K. Le Guin");
  EXPECT_EQ(parser.language, "en");
  EXPECT_EQ(Storage.writeOpens, 0);
  EXPECT_EQ(Storage.readOpens, 0);
}

TEST(ContentOpfParserMetadata, NeverEntersManifestWhenMetadataElementIsMissing) {
  const std::string xml =
      R"(<package><manifest><item id="chapter" href="chapter.xhtml"/></manifest><spine/></package>)";
  Storage = {};
  ContentOpfParser parser("/missing-cache", "OPS/", xml.size(), nullptr, /*collectCssFiles=*/true,
                          /*metadataOnly=*/true);

  ASSERT_TRUE(parser.setup());
  EXPECT_LT(parser.write(reinterpret_cast<const uint8_t*>(xml.data()), xml.size()), xml.size());
  EXPECT_EQ(Storage.writeOpens, 0);
  EXPECT_EQ(Storage.readOpens, 0);
}
