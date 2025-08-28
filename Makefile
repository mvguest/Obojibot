all:
	@g++ bot.cpp -o oboji -ldpp -lcurl -pthread
init:
	@g++ bot.cpp -o oboji -ldpp -lcurl -pthread
	@./oboji
	@rm -rf oboji
