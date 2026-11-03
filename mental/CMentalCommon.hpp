#pragma once


class DisableCopyAndMove
{
protected:
	DisableCopyAndMove() = default;
	~DisableCopyAndMove() = default;
public:
	DisableCopyAndMove(const DisableCopyAndMove&) = delete;
	DisableCopyAndMove& operator=(const DisableCopyAndMove&) = delete;
	DisableCopyAndMove(DisableCopyAndMove&&) = delete;
	DisableCopyAndMove& operator=(DisableCopyAndMove&&) = delete;
};

typedef enum EMentalFlags {
	MENTAL_FLAGS_ENABLE_GLEW_EXPERIMENTAL = 1,
} EMentalFlags;

typedef enum EMentalStatusCode {
	MENTAL_STATUS_CODE_GLEW_INIT_FAIL = -1,
	MENTAL_STATUS_CODE_OK = 0,
} EMentalStatusCode;

typedef struct MentalResult {
	EMentalStatusCode eStatusCode;
	const char* cpMessage;
} MentalResult;

MentalResult Ok(EMentalStatusCode eStatusCode, const char* cpMessage) {
	return MentalResult {.eStatusCode=eStatusCode, .cpMessage = cpMessage }
}

MentalResult Err(EMentalStatusCode eStatusCode, const char* cpMessage) {
	return Ok(eStatusCode, cpMessage);
}
