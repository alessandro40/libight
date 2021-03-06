#include <ight/ooni/net_test.hpp>
#include <ctime>

using namespace ight::common::settings;
using namespace ight::ooni::net_test;

NetTest::NetTest(std::string input_filepath_, Settings options_)
  : input_filepath(input_filepath_), options(options_),
    test_name("net_test"), test_version("0.0.1")
{
}

NetTest::NetTest(void) : NetTest::NetTest("", Settings())
{
  // nothing
}

NetTest::NetTest(std::string input_filepath_) : 
  NetTest::NetTest(input_filepath_, Settings())
{
  // nothing
}

NetTest::~NetTest()
{
    delete input;
    input = nullptr;
}

InputGenerator *
NetTest::input_generator()
{
  return new InputFileGenerator(input_filepath, logger);
}

std::string
NetTest::get_report_filename()
{
  std::string filename;
  char buffer[100];
  strftime(buffer, sizeof(buffer), "%FT%H%M%SZ",
           gmtime(&file_report.start_time));
  std::string iso_8601_date(buffer);
  filename = "report-" + file_report.test_name + "-";
  filename += iso_8601_date + ".yamloo";
  return filename;
}

void
NetTest::geoip_lookup()
{
  probe_ip = "127.0.0.1";
  probe_asn = "AS0";
  probe_cc = "ZZ";
}

void
NetTest::run_next_measurement(const std::function<void()>&& cb)
{
  logger->debug("net_test: running next measurement");
  input->next([=](std::string next_input) {
      logger->debug("net_test: creating entry");
      entry = ReportEntry(next_input);
      logger->debug("net_test: calling setup");
      setup();
      logger->debug("net_test: running with input %s", next_input.c_str());
      main(next_input, options, [=](ReportEntry entry) {
          logger->debug("net_test: tearing down");
          teardown();
          file_report.writeEntry(entry);
          logger->debug("net_test: written entry");
          logger->debug("net_test: increased");
          run_next_measurement(std::move(cb));
      }); 
  }, [=]() {
    logger->debug("net_test: reached end of input");
    cb();
  });
}

void
NetTest::begin(std::function<void()> cb)
{
  geoip_lookup();
  write_header();
  if (input_filepath != ""){
    logger->debug("net_test: found input file");
    if (input != nullptr) {
      delete input;
      input = nullptr;
    }
    input = input_generator();
    run_next_measurement(std::move(cb));
  } else {
    logger->debug("net_test: no input file");
    entry = ReportEntry();
    logger->debug("net_test: calling setup");
    setup();
    main(options, [=](ReportEntry entry) {
      logger->debug("net_test: tearing down");
      teardown();
      file_report.writeEntry(entry);
      logger->debug("net_test: written entry");
      logger->debug("net_test: reached end of input");
      cb();
    });
  }
}

void
NetTest::write_header()
{
  file_report.test_name = test_name;
  file_report.test_version = test_version;
  time(&file_report.start_time);

  file_report.options = options;
  file_report.filename = get_report_filename();

  file_report.probe_ip = probe_ip;
  file_report.probe_cc = probe_cc;
  file_report.probe_asn = probe_asn;

  file_report.open();
}

void
NetTest::end(std::function<void()> cb)
{
  file_report.close();
  cb();
}

void
NetTest::setup(std::string) {
}

void
NetTest::setup() {
}

void
NetTest::teardown(std::string) {
}

void
NetTest::teardown() {
}

void
NetTest::main(Settings,
              std::function<void(ReportEntry)>&& cb) {
  delayed_call = std::make_shared<DelayedCall>(1.25, [=](void) {
    ReportEntry entry;
    cb(entry);
  });
}

void
NetTest::main(std::string, Settings,
              std::function<void(ReportEntry)>&& cb) {
  delayed_call = std::make_shared<DelayedCall>(1.25, [=](void) {
    ReportEntry entry;
    cb(entry);
  });
}
