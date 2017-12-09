#### Пример
```tpk o=outatlas.png i=outidsandpositions.ini [image1id] /some/image.png [image2id] /some/image2.png```
### Опции
* h,--help - показать спарвку
* v,--version  - показать версию
* l,--log-loaded - показать загружаемые изображения и их 2d-размеры
* o,--out-image - файл для сохранения атласа(в фомате PNG)
* i,--out-ini - файл для сохранения информации о позициях изображений в атласе(в фомате INI)
* w,--max-width - максимальная ширина атласа в пикселах (w=1024)
* c,--in-conf - прочитать параметры из файла
```
cat somefile.ini

[image1id]
/some/image.png
[image2id]
/some/image2.png

tpk o=outatlas.png i=outidsandpositions.ini c=somefile.ini
```
