/*
 * %COPYRIGHT%
 *
 * %LICENSE%
 *
 * $Id$
 */

#include "albtest.h"

#include "logger.h"
#include "memory-alloc.h"

#include "packer.h"
#include "slab.h"
#include "slab-depot.h"
#include "slab-journal.h"
#include "vdo.h"

#include "adminUtils.h"
#include "asyncLayer.h"
#include "ioRequest.h"
#include "mutexUtils.h"
#include "packerUtils.h"
#include "slabSummaryUtils.h"
#include "vdoAsserts.h"
#include "vdoTestBase.h"

typedef struct {
  IORequest *request;
  char      *buffer;
} ReadRequest;

static const size_t DATA_BLOCKS = 1024 * 5;
static const size_t NUM_RUNS    = 512;

static const size_t WRITE_BATCH      = 4;
static const size_t DEDUPE_BATCH     = 4;
static const size_t OVERWRITE_BATCH  = 2;
static const size_t ZERO_BLOCK_BATCH = 2;
static const size_t READ_BATCH       = 4;

static IORequest **writeRequests;
static size_t      writeRequestCount;
static size_t      writeLaunched = 0;

static ReadRequest *readRequests;
static size_t       readRequestCount;
static size_t       readLaunched = 0;

static struct vdo_slab *slabToSave;
static bool             outputBinsIdle = false;

/**
 * Test-specific initialization.
 **/
static void initializeDedupeAndCompressT1(void)
{
  const TestParameters parameters = {
    .mappableBlocks      = DATA_BLOCKS * 2,
    .slabJournalBlocks   = 4,
    .journalBlocks       = 1024,
    .logicalThreadCount  = 3,
    .physicalThreadCount = 2,
    .hashZoneThreadCount = 2,
    .enableCompression   = true,
    .cacheSize           = 64,
  };
  initializeVDOTest(&parameters);

  writeLaunched  = 0;
  readLaunched   = 0;
  outputBinsIdle = false;

  size_t totalWritesPerRun
    = WRITE_BATCH + DEDUPE_BATCH + OVERWRITE_BATCH + ZERO_BLOCK_BATCH;
  writeRequestCount = totalWritesPerRun * NUM_RUNS;
  VDO_ASSERT_SUCCESS(UDS_ALLOCATE((writeRequestCount), IORequest *,
                                  "write requests", &writeRequests));

  readRequestCount = 2 * READ_BATCH * NUM_RUNS;
  VDO_ASSERT_SUCCESS(UDS_ALLOCATE((readRequestCount), ReadRequest,
                                  "read requests", &readRequests));

  for (size_t i = 0; i < readRequestCount; i++) {
    VDO_ASSERT_SUCCESS(UDS_ALLOCATE(VDO_BLOCK_SIZE, char,
                                    "read buffer",
                                    &readRequests[i].buffer));
  }
}

/**
 * Test-specific teardown.
 **/
static void tearDownDedupeAndCompressT1(void)
{
  for (size_t i = 0; i < readRequestCount; i++) {
    free(readRequests[i].buffer);
  }
  free(readRequests);
  free(writeRequests);
  tearDownVDOTest();
}

/**********************************************************************/
static void launchWrite(logical_block_number_t logical, block_count_t offset)
{
  writeRequests[writeLaunched++] = launchIndexedWrite(logical, 1, offset);
}

/**********************************************************************/
static void launchRead(logical_block_number_t logical)
{
  readRequests[readLaunched].request
    = launchBufferBackedRequest(logical, 1, readRequests[readLaunched].buffer,
                                REQ_OP_READ);
  readLaunched++;
}

/**
 * Simulate a VDO crash and restart it as dirty.
 */
static void crashAndRebuildVDO(void)
{
  crashVDO();
  startVDO(VDO_DIRTY);
  waitForRecoveryDone();
}

/**
 * Get a slab journal from a specific slab.
 *
 * @param  slabNumber  the slab number of the slab journal
 **/
static struct slab_journal *getVDOSlabJournal(slab_count_t slabNumber)
{
  return vdo->depot->slabs[slabNumber]->journal;
}

/**
 * Test vdo with a mix of read and write.
 **/
static void testReadWriteMix(void)
{
  size_t writeOffset     = 1;
  size_t overwriteOffset = 0;
  size_t zeroBlockOffset = 0;

  for (size_t iteration = 0; iteration < NUM_RUNS; iteration++) {
    // Batch write data.
    for (size_t batched = 0; batched < WRITE_BATCH; batched++) {
      launchWrite(writeLaunched, writeOffset);
      writeOffset++;
    }

    // Batch read data.
    for (size_t batched = 0; batched < READ_BATCH; batched++) {
      launchRead(readLaunched);
    }

    // Batch write duplicate data.
    for (size_t batched = 0; batched < DEDUPE_BATCH; batched++) {
      launchWrite(writeLaunched, writeOffset - 1);
    }

    // Batch read data.
    for (size_t batched = 0; batched < READ_BATCH; batched++) {
      launchRead(readLaunched);
    }

    // Batch overwrite existing blocks.
    for (size_t batched = 0; batched < OVERWRITE_BATCH; batched++) {
      launchWrite(overwriteOffset, overwriteOffset + 3);
      overwriteOffset++;
    }

    // Batch write zero blocks.
    for (size_t batched = 0; batched < ZERO_BLOCK_BATCH; batched++) {
      launchWrite(zeroBlockOffset * 2, 0);
      zeroBlockOffset++;
    }
  }

  // Wait for all reads to complete.
  for (size_t waiting = 0; waiting < readRequestCount; waiting++) {
    if (readRequests[waiting].request != NULL) {
      awaitAndFreeSuccessfulRequest(UDS_FORGET(readRequests[waiting].request));
    }
  }

  // Turn off compression to prevent further packing and then flush packer.
  performSetVDOCompressing(false);

  // Wait for all writes to complete.
  for (size_t waiting = 0; waiting < writeRequestCount; waiting++) {
    if (writeRequests[waiting] != NULL) {
      awaitAndFreeSuccessfulRequest(UDS_FORGET(writeRequests[waiting]));
    }
  }

  struct packer_statistics stats = vdo_get_packer_statistics(vdo->packer);
  CU_ASSERT_EQUAL(0, stats.compressed_fragments_in_packer);

  // Flush slab journals and refCounts. Mark them as dirty in the slab
  // summary to force slab scrubbing.
  performSuccessfulDepotAction(VDO_ADMIN_STATE_RECOVERING);

  struct slab_depot *depot = vdo->depot;
  struct slab_summary_zone *summaryZone
    = vdo_get_slab_summary_for_zone(depot->slab_summary, 0);
  for (slab_count_t i = 0; i < depot->slab_count; i++) {
    slabToSave = depot->slabs[i];
    performSuccessfulSlabAction(slabToSave,
                                VDO_ADMIN_STATE_SAVE_FOR_SCRUBBING);
    struct slab_journal *slabJournal
      = getVDOSlabJournal(slabToSave->slab_number);
    tail_block_offset_t tailBlockOffset
      = vdo_get_slab_journal_block_offset(slabJournal,
                                          slabJournal->last_summarized);
    bool loadRefCounts = vdo_must_load_ref_counts(summaryZone,
                                                  slabToSave->slab_number);
    performSlabSummaryUpdate(summaryZone, slabToSave->slab_number,
                             tailBlockOffset, loadRefCounts, false, 1000);
    CU_ASSERT_FALSE(vdo_get_summarized_cleanliness(summaryZone,
                                                   slabToSave->slab_number));
  }

  crashAndRebuildVDO();
}

/**********************************************************************/

static CU_TestInfo vdoTests[] = {
  { "Mixed compressible and dedupe data",  testReadWriteMix  },
  CU_TEST_INFO_NULL
};

static CU_SuiteInfo vdoSuite = {
  .name  = "VDO dedupe and compression tests (DedupeAndCompress_t1)",
  .initializerWithArguments = NULL,
  .initializer              = initializeDedupeAndCompressT1,
  .cleaner                  = tearDownDedupeAndCompressT1,
  .tests                    = vdoTests
};

CU_SuiteInfo *initializeModule(void)
{
  return &vdoSuite;
}
