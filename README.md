# arduino-remote-gsm-station
проект  умеет :

считывать данные с беспроводных датчиков с длительным временем работы

выводить их на дисплей

посылать по команде информацию с датчиков по смс по номеру первой ячейки сим

записывать в первую ячейку мастер номер и в дальнейшем реагировать на команды только с этого номера

на определенные датчики при превышении или уменьшении некого порога посылается смс сообщение

на sd карту должен ведется лог показаний датчиков

команды смс :

#info - краткая информация с датчиков

#inff - полная информация с датчиков -напряжение батареи и время прихода последних данных

#settime -установка текущего времени в модуль часов, время берется автоматически из служебных данных смс

#setmynum -установка мастер номера, с которого будут приниматься команды, с других номеров игнорироваться

#balans посылка текущего балнса


#all - вывести список всех уведомлений

#al*  - удалить все уведомления

#al1* удалить уведомление с заданным номером, где 1 - номер ячейки в массиве уведомлений

#al123u34.50 - в ячейку 1 от датчика 2 , а его сенсора 3 , u -признак того что увкедомление будет послано при превышении порга, d - при понижении порога, 34.50  сам порог срабатывания сенсора,

эти команды могут быть даны с последовательного порта 


проект должен еще уметь 

считывать показания датчиков, подключенных к основной плате

должна быть возможность подключится из интернета по wifi и посмотреть текущие показания

должна быть возможность прочитать лог датчиков с интернета


исходный код
https://create.arduino.cc/editor/sasha_ml/d271f25d-d158-4372-87bb-69d778120403/preview

схемы
https://easyeda.com/sasha_ml/Arduino_Mega-15a761a8028f4398b58b41b1c01d44cf
https://easyeda.com/normal/with_sim900-cec6111705ad414b99fb65d70da5dad2




