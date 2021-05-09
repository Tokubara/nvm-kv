# ---
# jupyter:
#   jupytext:
#     text_representation:
#       extension: .R
#       format_name: light
#       format_version: '1.5'
#       jupytext_version: 1.11.1
#   kernelspec:
#     display_name: R
#     language: R
#     name: ir
# ---

result_8<-read_csv("result_8.csv",col_names = c("threadNR","readNR","isSkewed","op"),col_types = list(col_integer(),col_integer(),col_factor(ordered=TRUE),col_double()))

plot_8<-qplot(result_8, aes(x=threadNR, y=op, color=isSkewed)) +
  geom_point() + 
  facet_grid(. ~ readNR)

result_32<-read_csv("result_32.csv",col_names = c("threadNR","readNR","isSkewed","op"),col_types = list(col_integer(),col_integer(),col_factor(ordered=TRUE),col_double()))

plot_32<-qplot(result_32, aes(x=threadNR, y=op, color=isSkewed)) +
  geom_point() + 
  facet_grid(. ~ readNR)+ggtitle("BUCKET_NUM=32")

gridExtra::grid.arrange(plot_8,plot_32)

result_32%>%dplyr::filter(isSkewed==1)%>%.$op
