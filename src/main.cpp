#include <filesystem>
#include <memory>

#include "Tests/Tests.h"

#include "XGD.h"
#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"

int main() {
    XGDLog().set_log_level(Debug);

    std::shared_ptr<ImageReader> image_reader = ReaderFactory::create(FileType::ISO, {"C:/Users/jackx/Documents/GitHub/extract-xiso/test_files/Harry Potter and the Prisoner of Azkaban (USA).iso"});
    std::shared_ptr<ImageWriter> image_writer = WriterFactory::create(FileType::GoD, image_reader, ScrubType::PARTIAL, false, false);

    image_writer->convert("C:/Users/jackx/Documents/GitHub/extract-xiso/test_files/Harry Potter and the Prisoner of Azkaban (USA) GOD TEST");
    
    // CCIWriter writer(image_reader, ScrubType::FULL, false);
    // writer.convert("C:/Users/jackx/Documents/GitHub/extract-xiso/test_files/Harry Potter and the Prisoner of Azkaban (USA) full scrub ITERATIVE 2.cci");

}